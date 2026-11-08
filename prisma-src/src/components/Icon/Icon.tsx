import { useMemo } from 'react';

interface IconProps {
    base64Data?: string;
    path?: string;
    size?: string;  // CSS size value like "4em", "64px", etc.
}

const PLACEHOLDER_ICON = './assets/icons/placeholder.png';

// Cache decoded DDS base64 -> PNG data URL to avoid re-decoding on scene navigation
const ddsPngCache = new Map<string, string>();
const MAX_CACHE_ENTRIES = 256;

// Precomputed CRC32 table for PNG chunk checksums
const CRC32_TABLE = (() => {
    const table = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) {
            c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
        }
        table[n] = c >>> 0;
    }
    return table;
})();

function crc32(bytes: Uint8Array, start: number, length: number): number {
    let c = 0xffffffff;
    const end = start + length;
    for (let i = start; i < end; i++) {
        c = CRC32_TABLE[(c ^ bytes[i]) & 0xff] ^ (c >>> 8);
    }
    return (c ^ 0xffffffff) >>> 0;
}

function uint8ToBase64(bytes: Uint8Array): string {
    const CHUNK = 0x8000;
    const parts: string[] = [];
    for (let i = 0; i < bytes.length; i += CHUNK) {
        const slice = bytes.subarray(i, Math.min(i + CHUNK, bytes.length));
        parts.push(String.fromCharCode.apply(null, slice as unknown as number[]));
    }
    return 'data:image/png;base64,' + btoa(parts.join(''));
}

// Encode raw RGBA8 pixels into a PNG data URL on the CPU.
// Bypasses HTMLCanvasElement.toDataURL(), which returns empty data on Ultralight 2.0's
// GPU-backed CanvasRecorder (where CanvasRecorder::surface() is null).
function encodeRgbaToPngDataUrl(width: number, height: number, rgba: Uint8Array): string {
    const rowStride = width * 4;
    const scanlineLen = 1 + rowStride; // 1 filter byte (0 = None) + RGBA row
    const rawDataLen = height * scanlineLen;

    // Zlib stream using uncompressed DEFLATE blocks (BTYPE=00), max 65535 bytes per block
    const maxBlockSize = 65535;
    const numBlocks = Math.ceil(rawDataLen / maxBlockSize) || 1;
    const zlibLen = 2 + rawDataLen + numBlocks * 5 + 4; // 2-byte header + blocks + 4-byte Adler32

    // Total PNG size:
    // 8 (signature) + 25 (IHDR) + (12 + zlibLen) (IDAT) + 12 (IEND)
    const totalLen = 8 + 25 + 12 + zlibLen + 12;
    const out = new Uint8Array(totalLen);
    const view = new DataView(out.buffer);
    let pos = 0;

    // 1. PNG Signature
    out.set([137, 80, 78, 71, 13, 10, 26, 10], pos);
    pos += 8;

    // 2. IHDR Chunk (length = 13)
    view.setUint32(pos, 13, false); pos += 4;
    const ihdrTypeStart = pos;
    out.set([73, 72, 68, 82], pos); pos += 4; // 'IHDR'
    view.setUint32(pos, width, false); pos += 4;
    view.setUint32(pos, height, false); pos += 4;
    out[pos++] = 8; // bit depth: 8
    out[pos++] = 6; // color type: 6 (RGBA)
    out[pos++] = 0; // compression: 0
    out[pos++] = 0; // filter: 0
    out[pos++] = 0; // interlace: 0
    view.setUint32(pos, crc32(out, ihdrTypeStart, 17), false); pos += 4;

    // 3. IDAT Chunk
    view.setUint32(pos, zlibLen, false); pos += 4;
    const idatTypeStart = pos;
    out.set([73, 68, 65, 84], pos); pos += 4; // 'IDAT'

    // Zlib header (CMF=0x78, FLG=0x01)
    out[pos++] = 0x78;
    out[pos++] = 0x01;

    // Build raw filtered scanlines and compute Adler-32 in one pass
    const rawScanlines = new Uint8Array(rawDataLen);
    let s1 = 1;
    let s2 = 0;
    let rawPos = 0;
    let srcPos = 0;

    for (let y = 0; y < height; y++) {
        rawScanlines[rawPos++] = 0; // Filter type 0 (None)
        s2 = (s2 + s1) % 65521;

        for (let i = 0; i < rowStride; i++) {
            const b = rgba[srcPos++];
            rawScanlines[rawPos++] = b;
            s1 += b;
            if (s1 >= 65521) s1 -= 65521;
            s2 += s1;
            if (s2 >= 65521) s2 -= 65521;
        }
    }

    // Write uncompressed DEFLATE blocks
    let remaining = rawDataLen;
    let scanOffset = 0;
    for (let b = 0; b < numBlocks; b++) {
        const blockLen = Math.min(remaining, maxBlockSize);
        const isFinal = (b === numBlocks - 1) ? 1 : 0;
        out[pos++] = isFinal;
        view.setUint16(pos, blockLen, true); pos += 2;
        view.setUint16(pos, (~blockLen) & 0xffff, true); pos += 2;
        out.set(rawScanlines.subarray(scanOffset, scanOffset + blockLen), pos);
        pos += blockLen;
        scanOffset += blockLen;
        remaining -= blockLen;
    }

    // Adler-32 checksum (big-endian)
    const adler = ((s2 << 16) | s1) >>> 0;
    view.setUint32(pos, adler, false); pos += 4;

    // IDAT CRC
    view.setUint32(pos, crc32(out, idatTypeStart, 4 + zlibLen), false); pos += 4;

    // 4. IEND Chunk (length = 0)
    view.setUint32(pos, 0, false); pos += 4;
    const iendTypeStart = pos;
    out.set([73, 69, 78, 68], pos); pos += 4; // 'IEND'
    view.setUint32(pos, crc32(out, iendTypeStart, 4), false); pos += 4;

    return uint8ToBase64(out);
}

// Convert base64 string to ArrayBuffer
function base64ToArrayBuffer(base64: string): ArrayBuffer {
    const binaryString = atob(base64);
    const bytes = new Uint8Array(binaryString.length);
    for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
    }
    return bytes.buffer;
}

function decodeDdsBase64(base64Data: string): string | null {
    const cached = ddsPngCache.get(base64Data);
    if (cached) {
        return cached;
    }

    try {
        const buffer = base64ToArrayBuffer(base64Data);
        const dataView = new DataView(buffer);

        // Parse DDS header
        const magic = dataView.getUint32(0, true);
        if (magic !== 0x20534444) { // 'DDS '
            console.error('Not a valid DDS file');
            return null;
        }

        const headerHeight = dataView.getUint32(12, true);
        const headerWidth = dataView.getUint32(16, true);
        const fourCC = dataView.getUint32(84, true);

        // Header is 128 bytes
        const dataOffset = 128;
        const rgba = new Uint8Array(headerWidth * headerHeight * 4);

        // DXT1 (BC1) - FourCC = 'DXT1' = 0x31545844
        // DXT3 (BC2) - FourCC = 'DXT3' = 0x33545844
        // DXT5 (BC3) - FourCC = 'DXT5' = 0x35545844
        if (fourCC === 0x31545844) {
            decodeDXT1(buffer, dataOffset, headerWidth, headerHeight, rgba);
        } else if (fourCC === 0x33545844) {
            decodeDXT3(buffer, dataOffset, headerWidth, headerHeight, rgba);
        } else if (fourCC === 0x35545844) {
            decodeDXT5(buffer, dataOffset, headerWidth, headerHeight, rgba);
        } else {
            console.error('Unsupported DDS format:', fourCC.toString(16));
            return null;
        }

        const dataUrl = encodeRgbaToPngDataUrl(headerWidth, headerHeight, rgba);
        if (ddsPngCache.size >= MAX_CACHE_ENTRIES) {
            const oldestKey = ddsPngCache.keys().next().value;
            if (oldestKey !== undefined) {
                ddsPngCache.delete(oldestKey);
            }
        }
        ddsPngCache.set(base64Data, dataUrl);
        return dataUrl;
    } catch (err) {
        console.error('Failed to decode DDS:', err);
        return null;
    }
}

export const Icon = ({ base64Data, path = '', size = "4em" }: IconProps) => {
    const imageSrc = useMemo(() => {
        if (!base64Data) {
            return path || PLACEHOLDER_ICON;
        }
        return decodeDdsBase64(base64Data) || path || PLACEHOLDER_ICON;
    }, [base64Data, path]);

    return <img src={imageSrc} style={{ width: size, height: size }} alt="" />;
};

// DXT1 decoder
function decodeDXT1(buffer: ArrayBuffer, offset: number, width: number, height: number, output: Uint8Array) {
    const data = new DataView(buffer);
    const blocksX = Math.ceil(width / 4);
    const blocksY = Math.ceil(height / 4);

    for (let by = 0; by < blocksY; by++) {
        for (let bx = 0; bx < blocksX; bx++) {
            const blockOffset = offset + (by * blocksX + bx) * 8;
            const c0 = data.getUint16(blockOffset, true);
            const c1 = data.getUint16(blockOffset + 2, true);
            const indices = data.getUint32(blockOffset + 4, true);

            const colors = decodeColors565(c0, c1, c0 <= c1);

            for (let py = 0; py < 4; py++) {
                for (let px = 0; px < 4; px++) {
                    const x = bx * 4 + px;
                    const y = by * 4 + py;
                    if (x >= width || y >= height) continue;

                    const idx = (py * 4 + px) * 2;
                    const colorIdx = (indices >> idx) & 0x3;
                    const color = colors[colorIdx];

                    const outIdx = (y * width + x) * 4;
                    output[outIdx] = color[0];
                    output[outIdx + 1] = color[1];
                    output[outIdx + 2] = color[2];
                    output[outIdx + 3] = color[3];
                }
            }
        }
    }
}

// DXT3 decoder
function decodeDXT3(buffer: ArrayBuffer, offset: number, width: number, height: number, output: Uint8Array) {
    const data = new DataView(buffer);
    const blocksX = Math.ceil(width / 4);
    const blocksY = Math.ceil(height / 4);

    for (let by = 0; by < blocksY; by++) {
        for (let bx = 0; bx < blocksX; bx++) {
            const blockOffset = offset + (by * blocksX + bx) * 16;

            const c0 = data.getUint16(blockOffset + 8, true);
            const c1 = data.getUint16(blockOffset + 10, true);
            const indices = data.getUint32(blockOffset + 12, true);
            const colors = decodeColors565(c0, c1, false);

            for (let py = 0; py < 4; py++) {
                const alphaRow = data.getUint16(blockOffset + py * 2, true);
                for (let px = 0; px < 4; px++) {
                    const x = bx * 4 + px;
                    const y = by * 4 + py;
                    if (x >= width || y >= height) continue;

                    const pixelIdx = py * 4 + px;
                    const colorIdx = (indices >> (pixelIdx * 2)) & 0x3;
                    const alpha4 = (alphaRow >> (px * 4)) & 0xf;
                    const alpha = (alpha4 << 4) | alpha4;
                    const color = colors[colorIdx];

                    const outIdx = (y * width + x) * 4;
                    output[outIdx] = color[0];
                    output[outIdx + 1] = color[1];
                    output[outIdx + 2] = color[2];
                    output[outIdx + 3] = alpha;
                }
            }
        }
    }
}

// DXT5 decoder
function decodeDXT5(buffer: ArrayBuffer, offset: number, width: number, height: number, output: Uint8Array) {
    const data = new DataView(buffer);
    const blocksX = Math.ceil(width / 4);
    const blocksY = Math.ceil(height / 4);

    for (let by = 0; by < blocksY; by++) {
        for (let bx = 0; bx < blocksX; bx++) {
            const blockOffset = offset + (by * blocksX + bx) * 16;

            // Alpha block (8 bytes)
            const a0 = data.getUint8(blockOffset);
            const a1 = data.getUint8(blockOffset + 1);
            const alphaLo = data.getUint32(blockOffset + 2, true) & 0xffffff;
            const alphaHi = (data.getUint32(blockOffset + 5, true) >>> 0) & 0xffffff;

            const alphas = decodeAlphas(a0, a1);

            // Color block (8 bytes)
            const c0 = data.getUint16(blockOffset + 8, true);
            const c1 = data.getUint16(blockOffset + 10, true);
            const indices = data.getUint32(blockOffset + 12, true);

            const colors = decodeColors565(c0, c1, false);

            for (let py = 0; py < 4; py++) {
                for (let px = 0; px < 4; px++) {
                    const x = bx * 4 + px;
                    const y = by * 4 + py;
                    if (x >= width || y >= height) continue;

                    const pixelIdx = py * 4 + px;
                    const colorIdx = (indices >> (pixelIdx * 2)) & 0x3;
                    const bitPos = pixelIdx * 3;
                    const alphaIdx = bitPos < 24
                        ? (alphaLo >> bitPos) & 0x7
                        : (alphaHi >> (bitPos - 24)) & 0x7;

                    const color = colors[colorIdx];
                    const alpha = alphas[alphaIdx];

                    const outIdx = (y * width + x) * 4;
                    output[outIdx] = color[0];
                    output[outIdx + 1] = color[1];
                    output[outIdx + 2] = color[2];
                    output[outIdx + 3] = alpha;
                }
            }
        }
    }
}

function decodeColors565(c0: number, c1: number, hasTransparent: boolean): [number, number, number, number][] {
    const r0 = ((c0 >> 11) & 0x1f) * 255 / 31;
    const g0 = ((c0 >> 5) & 0x3f) * 255 / 63;
    const b0 = (c0 & 0x1f) * 255 / 31;

    const r1 = ((c1 >> 11) & 0x1f) * 255 / 31;
    const g1 = ((c1 >> 5) & 0x3f) * 255 / 63;
    const b1 = (c1 & 0x1f) * 255 / 31;

    const colors: [number, number, number, number][] = [
        [r0, g0, b0, 255],
        [r1, g1, b1, 255],
        [0, 0, 0, 255],
        [0, 0, 0, 255]
    ];

    if (hasTransparent) {
        colors[2] = [(r0 + r1) / 2, (g0 + g1) / 2, (b0 + b1) / 2, 255];
        colors[3] = [0, 0, 0, 0];
    } else {
        colors[2] = [(2 * r0 + r1) / 3, (2 * g0 + g1) / 3, (2 * b0 + b1) / 3, 255];
        colors[3] = [(r0 + 2 * r1) / 3, (g0 + 2 * g1) / 3, (b0 + 2 * b1) / 3, 255];
    }

    return colors;
}

function decodeAlphas(a0: number, a1: number): number[] {
    const alphas = [a0, a1, 0, 0, 0, 0, 0, 0];

    if (a0 > a1) {
        for (let i = 2; i < 8; i++) {
            alphas[i] = ((8 - i) * a0 + (i - 1) * a1) / 7;
        }
    } else {
        for (let i = 2; i < 6; i++) {
            alphas[i] = ((6 - i) * a0 + (i - 1) * a1) / 5;
        }
        alphas[6] = 0;
        alphas[7] = 255;
    }

    return alphas;
}
