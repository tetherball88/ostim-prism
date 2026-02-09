import { useEffect, useRef, useState } from 'react';

interface NavigationIconProps {
    base64Data?: string;
    size?: string;  // CSS size value like "4em", "64px", etc.
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

const PLACEHOLDER_ICON = './assets/icons/placeholder.png';

export const NavigationIcon = ({ base64Data, size = "4em" }: NavigationIconProps) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const [imageSrc, setImageSrc] = useState<string | null>(null);

    useEffect(() => {
        if (!base64Data) {
            setImageSrc(PLACEHOLDER_ICON);
            return;
        }

        const processDDS = (buffer: ArrayBuffer) => {
            try {
                const dataView = new DataView(buffer);

                // Parse DDS header
                const magic = dataView.getUint32(0, true);
                if (magic !== 0x20534444) { // 'DDS '
                    console.error('Not a valid DDS file');
                    return;
                }

                const headerHeight = dataView.getUint32(12, true);
                const headerWidth = dataView.getUint32(16, true);
                const fourCC = dataView.getUint32(84, true);

                // Header is 128 bytes
                const dataOffset = 128;

                const canvas = canvasRef.current;
                if (!canvas) return;

                canvas.width = headerWidth;
                canvas.height = headerHeight;
                const ctx = canvas.getContext('2d');
                if (!ctx) return;

                const imageData = ctx.createImageData(headerWidth, headerHeight);

                // Check if it's DXT1 (BC1) - FourCC = 'DXT1' = 0x31545844
                // Check if it's DXT5 (BC3) - FourCC = 'DXT5' = 0x35545844
                if (fourCC === 0x31545844) {
                    decodeDXT1(buffer, dataOffset, headerWidth, headerHeight, imageData.data);
                } else if (fourCC === 0x35545844) {
                    decodeDXT5(buffer, dataOffset, headerWidth, headerHeight, imageData.data);
                } else {
                    console.error('Unsupported DDS format:', fourCC.toString(16));
                    return;
                }

                ctx.putImageData(imageData, 0, 0);
                setImageSrc(canvas.toDataURL());
            } catch (err) {
                console.error('Failed to decode DDS:', err);
            }
        };

        const buffer = base64ToArrayBuffer(base64Data);
        processDDS(buffer);
    }, [base64Data]);

    return (
        <>
            <canvas ref={canvasRef} style={{ display: 'none' }} />
            {imageSrc && <img src={imageSrc} style={{ width: size, height: size }} alt="" />}
        </>
    );
};

// DXT1 decoder
function decodeDXT1(buffer: ArrayBuffer, offset: number, width: number, height: number, output: Uint8ClampedArray) {
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

// DXT5 decoder
function decodeDXT5(buffer: ArrayBuffer, offset: number, width: number, height: number, output: Uint8ClampedArray) {
    const data = new DataView(buffer);
    const blocksX = Math.ceil(width / 4);
    const blocksY = Math.ceil(height / 4);

    for (let by = 0; by < blocksY; by++) {
        for (let bx = 0; bx < blocksX; bx++) {
            const blockOffset = offset + (by * blocksX + bx) * 16;

            // Alpha block (8 bytes)
            const a0 = data.getUint8(blockOffset);
            const a1 = data.getUint8(blockOffset + 1);
            const alphaBits =
                data.getUint8(blockOffset + 2) |
                (data.getUint8(blockOffset + 3) << 8) |
                (data.getUint8(blockOffset + 4) << 16) |
                (data.getUint8(blockOffset + 5) << 24) |
                (data.getUint8(blockOffset + 6) * 0x100000000) |
                (data.getUint8(blockOffset + 7) * 0x10000000000);

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
                    const alphaIdx = (alphaBits / Math.pow(2, pixelIdx * 3)) & 0x7;

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