//! Data analysis and bit-stream construction (single segment per symbol).

use crate::tables::num_data_codewords;

const ALPHANUMERIC: &[u8] = b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Mode {
    Numeric,
    Alphanumeric,
    Byte,
}

impl Mode {
    pub fn detect(data: &[u8]) -> Mode {
        if !data.is_empty() && data.iter().all(u8::is_ascii_digit) {
            Mode::Numeric
        } else if !data.is_empty() && data.iter().all(|b| ALPHANUMERIC.contains(b)) {
            Mode::Alphanumeric
        } else {
            Mode::Byte
        }
    }

    fn indicator(self) -> u32 {
        match self {
            Mode::Numeric => 1,
            Mode::Alphanumeric => 2,
            Mode::Byte => 4,
        }
    }

    fn count_bits(self, ver: usize) -> usize {
        let group = if ver <= 9 { 0 } else if ver <= 26 { 1 } else { 2 };
        match self {
            Mode::Numeric => [10, 12, 14][group],
            Mode::Alphanumeric => [9, 11, 13][group],
            Mode::Byte => [8, 16, 16][group],
        }
    }

    /// Number of payload bits needed for `len` input characters.
    fn payload_bits(self, len: usize) -> usize {
        match self {
            Mode::Numeric => len / 3 * 10 + [0, 4, 7][len % 3],
            Mode::Alphanumeric => len / 2 * 11 + (len % 2) * 6,
            Mode::Byte => len * 8,
        }
    }
}

#[derive(Default)]
pub struct BitBuffer {
    pub bits: Vec<bool>,
}

impl BitBuffer {
    pub fn push(&mut self, value: u32, count: usize) {
        for i in (0..count).rev() {
            self.bits.push((value >> i) & 1 != 0);
        }
    }
}

/// Total bits (header + payload) for the data in a given version, or None if
/// the character count does not fit in the count field.
fn total_bits(mode: Mode, len: usize, ver: usize) -> Option<usize> {
    let cc = mode.count_bits(ver);
    if len >= (1usize << cc) {
        return None;
    }
    Some(4 + cc + mode.payload_bits(len))
}

/// Smallest version in 1..=40 that holds the data at the given ECC level.
pub fn pick_version(mode: Mode, len: usize, ecl: usize) -> Option<usize> {
    (1..=40).find(|&ver| match total_bits(mode, len, ver) {
        Some(bits) => bits <= num_data_codewords(ver, ecl) * 8,
        None => false,
    })
}

/// Builds the padded data codewords for the symbol.
pub fn build_codewords(mode: Mode, data: &[u8], ver: usize, ecl: usize) -> Vec<u8> {
    let mut bb = BitBuffer::default();
    bb.push(mode.indicator(), 4);
    bb.push(data.len() as u32, mode.count_bits(ver));

    match mode {
        Mode::Numeric => {
            for chunk in data.chunks(3) {
                let value = chunk.iter().fold(0u32, |acc, &d| acc * 10 + (d - b'0') as u32);
                bb.push(value, chunk.len() * 3 + 1);
            }
        }
        Mode::Alphanumeric => {
            let index = |c: u8| ALPHANUMERIC.iter().position(|&a| a == c).unwrap_or(0) as u32;
            for chunk in data.chunks(2) {
                if chunk.len() == 2 {
                    bb.push(index(chunk[0]) * 45 + index(chunk[1]), 11);
                } else {
                    bb.push(index(chunk[0]), 6);
                }
            }
        }
        Mode::Byte => {
            for &b in data {
                bb.push(b as u32, 8);
            }
        }
    }

    let capacity_bits = num_data_codewords(ver, ecl) * 8;
    debug_assert!(bb.bits.len() <= capacity_bits);

    // Terminator (up to 4 zero bits), then pad to a byte boundary.
    let terminator = (capacity_bits - bb.bits.len()).min(4);
    bb.push(0, terminator);
    let to_byte = (8 - bb.bits.len() % 8) % 8;
    bb.push(0, to_byte);

    // Alternating pad codewords until capacity is reached.
    let mut pad = 0xEC;
    while bb.bits.len() < capacity_bits {
        bb.push(pad, 8);
        pad ^= 0xEC ^ 0x11;
    }

    let mut out = vec![0u8; bb.bits.len() / 8];
    for (i, &bit) in bb.bits.iter().enumerate() {
        if bit {
            out[i >> 3] |= 1 << (7 - (i & 7));
        }
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn mode_detection() {
        assert_eq!(Mode::detect(b"0123456789"), Mode::Numeric);
        assert_eq!(Mode::detect(b"HELLO WORLD"), Mode::Alphanumeric);
        assert_eq!(Mode::detect(b"hello"), Mode::Byte);
        assert_eq!(Mode::detect(b""), Mode::Byte);
    }

    #[test]
    fn version_capacity_limits() {
        // Published limits: version 1-L holds 41 digits / 25 alphanumerics / 17 bytes.
        assert_eq!(pick_version(Mode::Numeric, 41, 0), Some(1));
        assert_eq!(pick_version(Mode::Numeric, 42, 0), Some(2));
        assert_eq!(pick_version(Mode::Alphanumeric, 25, 0), Some(1));
        assert_eq!(pick_version(Mode::Byte, 17, 0), Some(1));
        assert_eq!(pick_version(Mode::Byte, 18, 0), Some(2));
        // Version 40-L holds 2953 bytes; 40-H holds 1273.
        assert_eq!(pick_version(Mode::Byte, 2953, 0), Some(40));
        assert_eq!(pick_version(Mode::Byte, 2954, 0), None);
        assert_eq!(pick_version(Mode::Byte, 1273, 3), Some(40));
        assert_eq!(pick_version(Mode::Byte, 1274, 3), None);
    }

    #[test]
    fn hello_world_1m_codewords() {
        // Widely published worked example: "HELLO WORLD" at 1-M.
        let cw = build_codewords(Mode::Alphanumeric, b"HELLO WORLD", 1, 1);
        assert_eq!(
            cw,
            vec![0x20, 0x5B, 0x0B, 0x78, 0xD1, 0x72, 0xDC, 0x4D, 0x43, 0x40, 0xEC, 0x11, 0xEC, 0x11, 0xEC, 0x11]
        );
    }
}
