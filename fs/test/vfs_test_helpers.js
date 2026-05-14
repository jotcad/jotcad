/**
 * Helper to create a protocol-compliant VFS result from raw data.
 */
export function vfsResult(data, metadata = {}) {
    let bytes, encoding = metadata.encoding || 'bytes';
    if (data instanceof Uint8Array) {
        bytes = data;
    } else if (typeof data === 'string') {
        bytes = new TextEncoder().encode(data);
        encoding = metadata.encoding || 'string';
    } else if (data === null || data === undefined) {
        bytes = new Uint8Array();
        encoding = metadata.encoding || 'null';
    } else {
        bytes = new TextEncoder().encode(JSON.stringify(data));
        encoding = metadata.encoding || 'json';
    }
    return {
        stream: new ReadableStream({
            start(controller) {
                controller.enqueue(bytes);
                controller.close();
            }
        }),
        metadata: { state: 'AVAILABLE', ...metadata, encoding }
    };
}

/**
 * Creates a mock provider that returns a fixed value wrapped in a VFS result.
 */
export const mockOp = (val, metadata) => async () => vfsResult(val, metadata);
