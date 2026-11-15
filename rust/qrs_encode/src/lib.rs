//! QR Studio encoder: QR Code Model 2, versions 1-40, ECC levels L/M/Q/H.
//!
//! Exposes a small C ABI (see `gui/include/qrs/qrs_encode.h`). The caller owns
//! every buffer, so no allocation ever crosses the DLL boundary.

mod matrix;
mod reed_solomon;
mod segment;
mod tables;

use matrix::Matrix;
use segment::Mode;
use tables::{num_raw_data_modules, ECC_CODEWORDS_PER_BLOCK, NUM_ERROR_CORRECTION_BLOCKS};

pub const QRS_ENCODE_ABI_VERSION: u32 = 1;

pub const QRS_ENC_ERR_NULL: i32 = -1;
pub const QRS_ENC_ERR_ECL: i32 = -2;
pub const QRS_ENC_ERR_TOO_LONG: i32 = -3;
pub const QRS_ENC_ERR_BUFFER: i32 = -4;
pub const QRS_ENC_ERR_INTERNAL: i32 = -99;

#[derive(Debug, PartialEq, Eq)]
pub enum EncodeError {
    InvalidEcl,
    DataTooLong,
}

pub struct QrSymbol {
    pub version: usize,
    pub mask: u32,
    pub size: usize,
    /// Row-major, `true` = dark module. No quiet zone.
    pub modules: Vec<bool>,
}

fn add_ecc_and_interleave(data: &[u8], ver: usize, ecl: usize) -> Vec<u8> {
    let num_blocks = NUM_ERROR_CORRECTION_BLOCKS[ecl][ver] as usize;
    let ecc_len = ECC_CODEWORDS_PER_BLOCK[ecl][ver] as usize;
    let raw = num_raw_data_modules(ver) / 8;
    let num_short = num_blocks - raw % num_blocks;
    let short_len = raw / num_blocks;

    let divisor = reed_solomon::divisor(ecc_len);
    let mut blocks: Vec<Vec<u8>> = Vec::with_capacity(num_blocks);
    let mut k = 0;
    for i in 0..num_blocks {
        let dat_len = short_len - ecc_len + usize::from(i >= num_short);
        let mut block = data[k..k + dat_len].to_vec();
        k += dat_len;
        let ecc = reed_solomon::remainder(&block, &divisor);
        if i < num_short {
            block.push(0); // placeholder so every block has equal length
        }
        block.extend_from_slice(&ecc);
        blocks.push(block);
    }

    let mut out = Vec::with_capacity(raw);
    for i in 0..=short_len {
        for (j, block) in blocks.iter().enumerate() {
            // Skip the placeholder byte of the short blocks.
            if i != short_len - ecc_len || j >= num_short {
                out.push(block[i]);
            }
        }
    }
    debug_assert_eq!(out.len(), raw);
    out
}

/// Encodes `data` choosing the smallest version and the lowest-penalty mask.
pub fn encode(data: &[u8], ecl: usize) -> Result<QrSymbol, EncodeError> {
    if ecl > 3 {
        return Err(EncodeError::InvalidEcl);
    }
    let mode = Mode::detect(data);
    let ver = segment::pick_version(mode, data.len(), ecl).ok_or(EncodeError::DataTooLong)?;
    let codewords = segment::build_codewords(mode, data, ver, ecl);
    let all = add_ecc_and_interleave(&codewords, ver, ecl);

    let mut m = Matrix::new(ver);
    m.draw_function_patterns(ver, ecl);
    m.draw_codewords(&all);

    let mut best = (u32::MAX, 0u32);
    for mask in 0..8 {
        m.apply_mask(mask);
        m.draw_format_bits(ecl, mask);
        let p = m.penalty();
        if p < best.0 {
            best = (p, mask);
        }
        m.apply_mask(mask); // undo
    }
    m.apply_mask(best.1);
    m.draw_format_bits(ecl, best.1);

    Ok(QrSymbol { version: ver, mask: best.1, size: m.size, modules: m.modules })
}

// ---------------------------------------------------------------- C ABI ----

#[no_mangle]
pub extern "C" fn qrs_encode_abi_version() -> u32 {
    QRS_ENCODE_ABI_VERSION
}

/// Encodes `len` bytes at `data` into `out` (one byte per module, 1 = dark,
/// row-major, no quiet zone).
///
/// Returns the symbol side length in modules (21..=177) on success, or a
/// negative `QRS_ENC_ERR_*` code. `ecl`: 0 = L, 1 = M, 2 = Q, 3 = H.
/// A buffer of 177 * 177 bytes is always large enough.
///
/// # Safety
/// `data` must be readable for `len` bytes and `out` writable for `out_cap` bytes.
#[no_mangle]
pub unsafe extern "C" fn qrs_encode(
    data: *const u8,
    len: usize,
    ecl: i32,
    out: *mut u8,
    out_cap: usize,
) -> i32 {
    if out.is_null() || (data.is_null() && len != 0) {
        return QRS_ENC_ERR_NULL;
    }
    if !(0..=3).contains(&ecl) {
        return QRS_ENC_ERR_ECL;
    }
    let input: &[u8] = if len == 0 { &[] } else { std::slice::from_raw_parts(data, len) };

    let result = std::panic::catch_unwind(|| encode(input, ecl as usize));
    match result {
        Ok(Ok(sym)) => {
            if sym.modules.len() > out_cap {
                return QRS_ENC_ERR_BUFFER;
            }
            let dst = std::slice::from_raw_parts_mut(out, sym.modules.len());
            for (d, &m) in dst.iter_mut().zip(&sym.modules) {
                *d = u8::from(m);
            }
            sym.size as i32
        }
        Ok(Err(EncodeError::InvalidEcl)) => QRS_ENC_ERR_ECL,
        Ok(Err(EncodeError::DataTooLong)) => QRS_ENC_ERR_TOO_LONG,
        Err(_) => QRS_ENC_ERR_INTERNAL,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hello_world_1m_interleaved_ecc() {
        // Published worked example: ECC codewords for "HELLO WORLD" at 1-M.
        let cw = segment::build_codewords(Mode::Alphanumeric, b"HELLO WORLD", 1, 1);
        let all = add_ecc_and_interleave(&cw, 1, 1);
        assert_eq!(&all[16..], &[196, 35, 39, 119, 235, 215, 231, 226, 93, 23]);
    }

    #[test]
    fn every_version_and_level_encodes() {
        for ecl in 0..4 {
            for ver in 1..=40 {
                let cap = tables::num_data_codewords(ver, ecl);
                // Fill most of the byte capacity of this version.
                let len = cap.saturating_sub(3).max(1);
                let data: Vec<u8> = (0..len).map(|i| b'a' + (i % 26) as u8).collect();
                let sym = encode(&data, ecl).expect("encodes");
                assert_eq!(sym.version, ver, "ecl {ecl}");
                assert_eq!(sym.size, ver * 4 + 17);
                assert_eq!(sym.modules.len(), sym.size * sym.size);
            }
        }
    }

    #[test]
    fn rejects_oversized_and_bad_level() {
        assert_eq!(encode(&vec![b'x'; 2954], 0).err(), Some(EncodeError::DataTooLong));
        assert_eq!(encode(b"x", 4).err(), Some(EncodeError::InvalidEcl));
    }

    #[test]
    fn ffi_contract() {
        let mut buf = vec![0u8; 177 * 177];
        let text = b"https://example.com";
        unsafe {
            let n = qrs_encode(text.as_ptr(), text.len(), 1, buf.as_mut_ptr(), buf.len());
            assert_eq!(n, 25); // version 2
            assert!(buf[..(n * n) as usize].iter().all(|&b| b <= 1));
            // Top-left finder: 7 dark modules then the light separator.
            assert_eq!(&buf[..8], &[1, 1, 1, 1, 1, 1, 1, 0]);

            assert_eq!(qrs_encode(text.as_ptr(), text.len(), 9, buf.as_mut_ptr(), buf.len()), QRS_ENC_ERR_ECL);
            assert_eq!(qrs_encode(text.as_ptr(), text.len(), 1, buf.as_mut_ptr(), 10), QRS_ENC_ERR_BUFFER);
            assert_eq!(qrs_encode(std::ptr::null(), 3, 1, buf.as_mut_ptr(), buf.len()), QRS_ENC_ERR_NULL);
            assert_eq!(qrs_encode(text.as_ptr(), text.len(), 1, std::ptr::null_mut(), 0), QRS_ENC_ERR_NULL);
        }
    }
}
