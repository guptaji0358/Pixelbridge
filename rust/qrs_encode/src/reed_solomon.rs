//! Reed-Solomon ECC over GF(2^8) with the QR polynomial 0x11D.

fn mul(x: u8, y: u8) -> u8 {
    let mut z: u32 = 0;
    for i in (0..8).rev() {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y as u32 >> i) & 1) * x as u32;
    }
    z as u8
}

/// Generator polynomial of the given degree (leading coefficient omitted).
pub fn divisor(degree: usize) -> Vec<u8> {
    let mut result = vec![0u8; degree];
    result[degree - 1] = 1;
    let mut root: u8 = 1;
    for _ in 0..degree {
        for j in 0..degree {
            result[j] = mul(result[j], root);
            if j + 1 < degree {
                result[j] ^= result[j + 1];
            }
        }
        root = mul(root, 2);
    }
    result
}

/// ECC codewords for one data block.
pub fn remainder(data: &[u8], divisor: &[u8]) -> Vec<u8> {
    let mut result = vec![0u8; divisor.len()];
    for &b in data {
        let factor = b ^ result[0];
        result.rotate_left(1);
        let last = result.len() - 1;
        result[last] = 0;
        for (r, &d) in result.iter_mut().zip(divisor) {
            *r ^= mul(d, factor);
        }
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn gf_multiplication_basics() {
        assert_eq!(mul(0, 77), 0);
        assert_eq!(mul(1, 77), 77);
        assert_eq!(mul(2, 0x80), 0x1D); // wraps through the field polynomial
    }

    #[test]
    fn known_generator_degree_7() {
        // Published generator for 7 ECC codewords (alpha exponents 87,229,146,149,238,102,21).
        assert_eq!(divisor(7), vec![127, 122, 154, 164, 11, 68, 117]);
    }
}
