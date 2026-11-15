//! Module placement: function patterns, codeword zigzag, masking and penalty scoring.

use crate::tables::ECC_FORMAT_BITS;

pub struct Matrix {
    pub size: usize,
    pub modules: Vec<bool>,
    function: Vec<bool>,
}

impl Matrix {
    pub fn new(ver: usize) -> Matrix {
        let size = ver * 4 + 17;
        Matrix { size, modules: vec![false; size * size], function: vec![false; size * size] }
    }

    fn get(&self, x: usize, y: usize) -> bool {
        self.modules[y * self.size + x]
    }

    fn set_function(&mut self, x: usize, y: usize, dark: bool) {
        let i = y * self.size + x;
        self.modules[i] = dark;
        self.function[i] = true;
    }

    pub fn draw_function_patterns(&mut self, ver: usize, ecl: usize) {
        let size = self.size;
        for i in 0..size {
            self.set_function(6, i, i % 2 == 0);
            self.set_function(i, 6, i % 2 == 0);
        }

        self.draw_finder(3, 3);
        self.draw_finder(size as i32 - 4, 3);
        self.draw_finder(3, size as i32 - 4);

        let align = alignment_positions(ver);
        let n = align.len();
        for i in 0..n {
            for j in 0..n {
                let corner = (i == 0 && j == 0) || (i == 0 && j == n - 1) || (i == n - 1 && j == 0);
                if !corner {
                    self.draw_alignment(align[i], align[j]);
                }
            }
        }

        // Reserve the format area with a dummy mask; overwritten later.
        self.draw_format_bits(ecl, 0);
        self.draw_version(ver);
    }

    fn draw_finder(&mut self, cx: i32, cy: i32) {
        let size = self.size as i32;
        for dy in -4..=4i32 {
            for dx in -4..=4i32 {
                let (x, y) = (cx + dx, cy + dy);
                if x >= 0 && x < size && y >= 0 && y < size {
                    let dist = dx.abs().max(dy.abs());
                    self.set_function(x as usize, y as usize, dist != 2 && dist != 4);
                }
            }
        }
    }

    fn draw_alignment(&mut self, cx: usize, cy: usize) {
        for dy in -2..=2i32 {
            for dx in -2..=2i32 {
                let dark = dx.abs().max(dy.abs()) != 1;
                self.set_function((cx as i32 + dx) as usize, (cy as i32 + dy) as usize, dark);
            }
        }
    }

    pub fn draw_format_bits(&mut self, ecl: usize, mask: u32) {
        let data = (ECC_FORMAT_BITS[ecl] << 3) | mask;
        let mut rem = data;
        for _ in 0..10 {
            rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        }
        let bits = ((data << 10) | rem) ^ 0x5412;
        let bit = |i: usize| (bits >> i) & 1 != 0;
        let size = self.size;

        // First copy, around the top-left finder.
        for i in 0..6 {
            self.set_function(8, i, bit(i));
        }
        self.set_function(8, 7, bit(6));
        self.set_function(8, 8, bit(7));
        self.set_function(7, 8, bit(8));
        for i in 9..15 {
            self.set_function(14 - i, 8, bit(i));
        }

        // Second copy, split between the other two finders.
        for i in 0..8 {
            self.set_function(size - 1 - i, 8, bit(i));
        }
        for i in 8..15 {
            self.set_function(8, size - 15 + i, bit(i));
        }
        self.set_function(8, size - 8, true); // always-dark module
    }

    fn draw_version(&mut self, ver: usize) {
        if ver < 7 {
            return;
        }
        let mut rem = ver as u32;
        for _ in 0..12 {
            rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        }
        let bits = ((ver as u32) << 12) | rem;
        for i in 0..18 {
            let dark = (bits >> i) & 1 != 0;
            let a = self.size - 11 + i % 3;
            let b = i / 3;
            self.set_function(a, b, dark);
            self.set_function(b, a, dark);
        }
    }

    /// Places the interleaved data+ECC codewords in the zigzag scan order.
    pub fn draw_codewords(&mut self, data: &[u8]) {
        let size = self.size as i32;
        let total = data.len() * 8;
        let mut i = 0usize;
        let mut right = size - 1;
        while right >= 1 {
            if right == 6 {
                right = 5;
            }
            for vert in 0..size {
                for j in 0..2 {
                    let x = (right - j) as usize;
                    let upward = (right + 1) & 2 == 0;
                    let y = (if upward { size - 1 - vert } else { vert }) as usize;
                    let idx = y * self.size + x;
                    if !self.function[idx] && i < total {
                        self.modules[idx] = (data[i >> 3] >> (7 - (i & 7))) & 1 != 0;
                        i += 1;
                    }
                }
            }
            right -= 2;
        }
    }

    /// XORs the mask pattern over the data modules (self-inverse).
    pub fn apply_mask(&mut self, mask: u32) {
        for y in 0..self.size {
            for x in 0..self.size {
                let invert = match mask {
                    0 => (x + y) % 2 == 0,
                    1 => y % 2 == 0,
                    2 => x % 3 == 0,
                    3 => (x + y) % 3 == 0,
                    4 => (x / 3 + y / 2) % 2 == 0,
                    5 => x * y % 2 + x * y % 3 == 0,
                    6 => (x * y % 2 + x * y % 3) % 2 == 0,
                    _ => ((x + y) % 2 + x * y % 3) % 2 == 0,
                };
                let idx = y * self.size + x;
                if invert && !self.function[idx] {
                    self.modules[idx] = !self.modules[idx];
                }
            }
        }
    }

    /// ISO 18004 penalty score (rules N1..N4); lower is better.
    pub fn penalty(&self) -> u32 {
        let size = self.size;
        let mut score = 0u32;

        // N1 (runs of 5+) and N3 (finder-like 1:1:3:1:1 with 4 light modules), per line.
        let mut line = vec![false; size];
        for pass in 0..2 {
            for a in 0..size {
                for b in 0..size {
                    line[b] = if pass == 0 { self.get(b, a) } else { self.get(a, b) };
                }
                score += line_penalty(&line);
            }
        }

        // N2: 2x2 blocks of one colour.
        for y in 0..size - 1 {
            for x in 0..size - 1 {
                let c = self.get(x, y);
                if c == self.get(x + 1, y) && c == self.get(x, y + 1) && c == self.get(x + 1, y + 1) {
                    score += 3;
                }
            }
        }

        // N4: dark/light balance, 10 points per 5% deviation from 50%.
        let dark = self.modules.iter().filter(|&&m| m).count();
        let total = size * size;
        let k = ((dark * 20).abs_diff(total * 10) + total - 1) / total;
        score += (k.max(1) as u32 - 1) * 10;
        score
    }
}

fn line_penalty(line: &[bool]) -> u32 {
    const P1: [bool; 11] = [true, false, true, true, true, false, true, false, false, false, false];
    const P2: [bool; 11] = [false, false, false, false, true, false, true, true, true, false, true];
    let mut score = 0;

    let mut run = 1;
    for i in 1..line.len() {
        if line[i] == line[i - 1] {
            run += 1;
            if run == 5 {
                score += 3;
            } else if run > 5 {
                score += 1;
            }
        } else {
            run = 1;
        }
    }

    if line.len() >= 11 {
        for w in line.windows(11) {
            if w == P1 || w == P2 {
                score += 40;
            }
        }
    }
    score
}

/// Centre coordinates of alignment patterns for a version (empty for version 1).
pub fn alignment_positions(ver: usize) -> Vec<usize> {
    if ver == 1 {
        return Vec::new();
    }
    let num = ver / 7 + 2;
    let step = if ver == 32 { 26 } else { (ver * 4 + num * 2 + 1) / (num * 2 - 2) * 2 };
    let size = ver * 4 + 17;
    let mut result = vec![6usize];
    let mut pos = size - 7;
    while result.len() < num {
        result.insert(1, pos);
        pos = pos.saturating_sub(step); // unused after the final insert
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn alignment_tables_match_spec() {
        assert!(alignment_positions(1).is_empty());
        assert_eq!(alignment_positions(2), vec![6, 18]);
        assert_eq!(alignment_positions(7), vec![6, 22, 38]);
        assert_eq!(alignment_positions(32), vec![6, 34, 60, 86, 112, 138]);
        assert_eq!(alignment_positions(40), vec![6, 30, 58, 86, 114, 142, 170]);
    }

    #[test]
    fn function_module_count_matches_capacity() {
        // Non-function modules must equal the raw data module count for every version.
        for ver in 1..=40 {
            let mut m = Matrix::new(ver);
            m.draw_function_patterns(ver, 0);
            let free = m.function.iter().filter(|&&f| !f).count();
            assert_eq!(free, crate::tables::num_raw_data_modules(ver), "version {ver}");
        }
    }
}
