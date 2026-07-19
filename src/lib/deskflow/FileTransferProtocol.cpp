/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferProtocol.h"

#include <cstring>

namespace deskflow {

void FileTransferProtocol::writeUInt32(std::string &buf, uint32_t value)
{
  uint8_t bytes[4];
  bytes[0] = (value >> 24) & 0xFF;
  bytes[1] = (value >> 16) & 0xFF;
  bytes[2] = (value >> 8) & 0xFF;
  bytes[3] = value & 0xFF;
  buf.append(reinterpret_cast<const char *>(bytes), 4);
}

void FileTransferProtocol::writeUInt64(std::string &buf, uint64_t value)
{
  writeUInt32(buf, static_cast<uint32_t>(value >> 32));
  writeUInt32(buf, static_cast<uint32_t>(value & 0xFFFFFFFF));
}

void FileTransferProtocol::writeString(std::string &buf, const std::string &str)
{
  writeUInt32(buf, static_cast<uint32_t>(str.size()));
  buf.append(str);
}

bool FileTransferProtocol::readUInt32(const char *&ptr, const char *end, uint32_t &value)
{
  if (ptr + 4 > end) {
    return false;
  }
  value = (static_cast<uint8_t>(ptr[0]) << 24) | (static_cast<uint8_t>(ptr[1]) << 16) |
          (static_cast<uint8_t>(ptr[2]) << 8) | static_cast<uint8_t>(ptr[3]);
  ptr += 4;
  return true;
}

bool FileTransferProtocol::readUInt64(const char *&ptr, const char *end, uint64_t &value)
{
  uint32_t hi, lo;
  if (!readUInt32(ptr, end, hi) || !readUInt32(ptr, end, lo)) {
    return false;
  }
  value = (static_cast<uint64_t>(hi) << 32) | lo;
  return true;
}

bool FileTransferProtocol::readString(const char *&ptr, const char *end, std::string &str)
{
  uint32_t len;
  if (!readUInt32(ptr, end, len)) {
    return false;
  }
  if (len > 1024 * 1024) { // Sanity check: max 1MB string
    return false;
  }
  if (ptr + len > end) {
    return false;
  }
  str.assign(ptr, len);
  ptr += len;
  return true;
}

bool FileTransferProtocol::serializeDragInfo(
    const DragInformation &dragInfo, std::string &outManifest, uint32_t &outTransferId
)
{
  if (!dragInfo.isValid()) {
    return false;
  }

  outTransferId = 0; // Single transfer ID for simplicity
  outManifest.clear();

  for (const auto &file : dragInfo.getFiles()) {
    writeString(outManifest, file.filename);
    writeUInt64(outManifest, file.size);
    writeString(outManifest, file.mimeType);
  }

  return true;
}

bool FileTransferProtocol::deserializeDragInfo(
    const std::string &manifest, uint32_t fileCount, DragInformation &outDragInfo
)
{
  outDragInfo.clear();

  const char *ptr = manifest.data();
  const char *end = ptr + manifest.size();

  for (uint32_t i = 0; i < fileCount; ++i) {
    std::string filename, mimeType;
    uint64_t size;

    if (!readString(ptr, end, filename) || !readUInt64(ptr, end, size) ||
        !readString(ptr, end, mimeType)) {
      return false;
    }

    if (!outDragInfo.addFile(filename, size, mimeType)) {
      return false;
    }
  }

  return ptr == end; // Must consume entire manifest
}

bool FileTransferProtocol::serializeFileChunk(const FileChunk &chunk, std::string &outPayload)
{
  if (!chunk.isValid()) {
    return false;
  }

  outPayload.clear();
  writeUInt32(outPayload, chunk.getFileIndex());
  writeUInt64(outPayload, chunk.getOffset());
  writeUInt32(outPayload, static_cast<uint32_t>(chunk.getSize()));
  outPayload.append(
      reinterpret_cast<const char *>(chunk.getData().data()), chunk.getSize()
  );

  return true;
}

bool FileTransferProtocol::deserializeFileChunk(const std::string &payload, FileChunk &outChunk)
{
  const char *ptr = payload.data();
  const char *end = ptr + payload.size();

  uint32_t fileIndex, dataLen;
  uint64_t offset;

  if (!readUInt32(ptr, end, fileIndex) || !readUInt64(ptr, end, offset) ||
      !readUInt32(ptr, end, dataLen)) {
    return false;
  }

  if (dataLen > FileChunk::MAX_CHUNK_SIZE) {
    return false;
  }

  if (ptr + dataLen != end) { // Must match exactly
    return false;
  }

  outChunk.setData(fileIndex, offset, reinterpret_cast<const uint8_t *>(ptr), dataLen);
  return true;
}

} // namespace deskflow
