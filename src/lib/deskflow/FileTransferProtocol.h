/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/DragInformation.h"
#include "deskflow/FileChunk.h"

#include <string>
#include <vector>

namespace deskflow {

/**
 * @brief Protocol serialization helpers for file transfer
 *
 * Converts DragInformation and FileChunk objects to/from wire format
 * for kMsgDDragInfo (DDRG) and kMsgDFileTransfer (DFTR) messages.
 *
 * @since Protocol version 1.9
 */
class FileTransferProtocol
{
public:
  /**
   * @brief Serialize DragInformation to DDRG manifest format
   *
   * Format (per file):
   * - Filename length (4 bytes) + UTF-8 string
   * - File size (8 bytes, uint64_t)
   * - MIME type length (4 bytes) + UTF-8 string
   *
   * @param dragInfo Drag information to serialize
   * @param outManifest Output manifest blob
   * @param outTransferId Output transfer ID (always 0 for now)
   * @return true if serialization succeeded
   */
  static bool serializeDragInfo(
      const DragInformation &dragInfo,
      std::string &outManifest,
      uint32_t &outTransferId
  );

  /**
   * @brief Deserialize DDRG manifest to DragInformation
   *
   * @param manifest Manifest blob from DDRG message
   * @param fileCount Expected file count from DDRG header
   * @param outDragInfo Output DragInformation object
   * @return true if parsing succeeded
   */
  static bool deserializeDragInfo(
      const std::string &manifest,
      uint32_t fileCount,
      DragInformation &outDragInfo
  );

  /**
   * @brief Serialize FileChunk to DFTR payload
   *
   * Format:
   * - File index (4 bytes, uint32_t)
   * - Chunk offset (8 bytes, uint64_t)
   * - Data length (4 bytes, uint32_t)
   * - Data (variable, raw bytes)
   *
   * @param chunk File chunk to serialize
   * @param outPayload Output payload blob
   * @return true if serialization succeeded
   */
  static bool serializeFileChunk(const FileChunk &chunk, std::string &outPayload);

  /**
   * @brief Deserialize DFTR payload to FileChunk
   *
   * @param payload Payload from DFTR message
   * @param outChunk Output FileChunk object
   * @return true if parsing succeeded
   */
  static bool deserializeFileChunk(const std::string &payload, FileChunk &outChunk);

private:
  static void writeUInt32(std::string &buf, uint32_t value);
  static void writeUInt64(std::string &buf, uint64_t value);
  static void writeString(std::string &buf, const std::string &str);

  static bool readUInt32(const char *&ptr, const char *end, uint32_t &value);
  static bool readUInt64(const char *&ptr, const char *end, uint64_t &value);
  static bool readString(const char *&ptr, const char *end, std::string &str);
};

} // namespace deskflow
