import { Shape } from './shape.js';
import {
  DecodeInexactGeometryText,
  EncodeInexactGeometryText,
} from './geometry.js';
import { cgal } from './getCgal.js';
import { makeAbsolute } from './makeAbsolute.js';
import { makeShape } from './shape.js';

// New helper function for text-based serialization
function serializeTextFileEntry(fileName, fileContent) {
  // File content length (as a string, not padded)
  const fileContentLengthStr = String(fileContent.length);

  // Format: \n=CTLEN FILENAME\nCONTENT
  return `\n=${fileContentLengthStr} ${fileName}\n${fileContent}`;
}

export const fromJot = async (
  assets,
  assetText,
  mainShapeFilename = 'files/main.jot'
) => {
  const deserializedAssets = new Map();
  let offset = 0;

  // Consume the initial newline if present (from the first entry)
  if (assetText[offset] === '\n') {
    offset++;
  }

  while (offset < assetText.length) {
    // Expecting '=CTLEN FILENAME\n'
    if (assetText[offset] !== '=') {
      throw new Error(
        `Expected '=' at offset ${offset} but got '${assetText[offset]}'`
      );
    }
    offset++; // Skip '='

    const headerLineEnd = assetText.indexOf('\n', offset);
    if (headerLineEnd === -1) {
      throw new Error(`Expected newline after header at offset ${offset}`);
    }
    const headerLine = assetText.substring(offset, headerLineEnd);
    offset = headerLineEnd + 1; // Move past newline

    const firstSpace = headerLine.indexOf(' ');
    if (firstSpace === -1) {
      throw new Error(`Expected space in header line '${headerLine}'`);
    }

    const fileContentLengthStr = headerLine.substring(0, firstSpace);
    const fileContentLength = parseInt(fileContentLengthStr, 10);
    if (isNaN(fileContentLength)) {
      throw new Error(
        `Invalid content length in header: '${fileContentLengthStr}'`
      );
    }

    const fileName = headerLine.substring(firstSpace + 1);

    const fileContent = assetText.substring(offset, offset + fileContentLength);
    offset += fileContentLength;

    deserializedAssets.set(fileName, fileContent);

    // After consuming content, expect a newline for the next entry's header
    if (offset < assetText.length && assetText[offset] === '\n') {
      offset++;
    }
  }

  // Re-register all assets (except the main Shape's filename) with the assets system
  for (const [fileName, fileContent] of deserializedAssets.entries()) {
    // If the filename indicates it's a geometry asset (e.g., starts with 'assets/text/')
    if (fileName !== mainShapeFilename && fileName.startsWith('assets/text/')) {
      // Extract the TextId from the path
      const geometryId = fileName.substring('assets/text/'.length);
      assets.textId(fileContent); // Re-register the content with its TextId
    }
  }

  // Now, get the main Shape object JSON string from the deserialized assets
  const mainJotJson = deserializedAssets.get(mainShapeFilename);
  if (!mainJotJson) {
    throw new Error(
      `Main JOT Shape data with filename '${mainShapeFilename}' not found in serialized data.`
    );
  }

  const plainShape = JSON.parse(mainJotJson);
  const reconstructedShape = Shape.fromPlain(plainShape);

  return reconstructedShape;
};

export const toJot = async (assets, shape, filename = 'files/main.jot') => {
  const collectedAssetContents = new Map(); // Map to store TextId -> content

  // Function to convert a Shape to a plain object for JSON serialization
  // and collect all referenced geometry TextIds.
  const shapeToPlainAndCollectAssets = (currentShape) => {
    const plainShape = {};
    if (currentShape.geometry) {
      const geometryId = currentShape.geometry;
      const assetPath = `assets/text/${geometryId}`; // Use asset path as the key
      if (!collectedAssetContents.has(assetPath)) {
        collectedAssetContents.set(assetPath, assets.getText(geometryId));
      }
      plainShape.geometry = geometryId; // Still store the ID in the Shape object
    }
    if (currentShape.mask) {
      plainShape.mask = currentShape.mask; // Assuming mask is already an ID or simple value
    }
    if (currentShape.shapes) {
      plainShape.shapes = currentShape.shapes.map((s) =>
        shapeToPlainAndCollectAssets(s)
      );
    }
    if (currentShape.tags) {
      plainShape.tags = currentShape.tags;
    }
    if (currentShape.tf) {
      plainShape.tf = currentShape.tf;
    }
    return plainShape;
  };

  const plainShape = shapeToPlainAndCollectAssets(shape);
  const mainJotJson = JSON.stringify(plainShape);

  // Add the main Shape object as a special asset with the provided filename
  collectedAssetContents.set(filename, mainJotJson);

  const serializedEntries = [];
  // Ensure the main Shape's entry is always the first entry for consistent deserialization
  serializedEntries.push(
    serializeTextFileEntry(filename, collectedAssetContents.get(filename))
  );
  for (const [name, content] of collectedAssetContents.entries()) {
    if (name !== filename) {
      // Avoid re-adding the main Shape's entry
      serializedEntries.push(serializeTextFileEntry(name, content));
    }
  }

  // Join all serialized entries without a separator
  return serializedEntries.join('');
};
