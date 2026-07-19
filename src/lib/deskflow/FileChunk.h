/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace deskflow {

/**
 * @brief File data chunk for streaming transfer
 *
 * Represents a single chunk of file data transmitted via kMsgDFileTransfer (DFTR).
 * Files are split into chunks to avoid blocking the network and allow progress tracking.
 *
 * **Wire Format** (kMsgDFileTransfer):
 * - File index (4 bytes, uint32_t) - index in DragInformation file list
 * - Chunk offset (8 bytes, uint64_t) - byte offset in file
 * - Data length (4 bytes, uint32_t) - chunk size
 * - Data (variable, raw bytes)
 *
 * **Constraints**:
 * - Default chunk size: 64 KB (balances throughput and latency)
 * - Max chunk size: 1 MB (to prevent oversized network packets)
 * - Last chunk may be smaller than chunk size
 *
 * **Usage Pattern**:
 * 1. Client reads file in chunks
 * 2. Each chunk sent as separate DFTR message
 * 3. Server reassembles chunks by offset
 * 4. Client sends DFDN when all chunks sent
 *
 * @see kMsgDFileTransfer, DragInformation
 * @since Protocol version 1.9
 */
class FileChunk
{
public:
  /**
   * @brief Create empty chunk
   */
  FileChunk() : m_fileIndex(0), m_offset(0) {}

  /**
   * @brief Create chunk with data
   * @param fileIndex Index in DragInformation file list (0-based)
   * @param offset Byte offset in file
   * @param data Chunk data (copied)
   * @param size Data size in bytes
   */
  FileChunk(uint32_t fileIndex, uint64_t offset, const uint8_t *data, size_t size);

  /**
   * @brief Get file index this chunk belongs to
   */
  uint32_t getFileIndex() const { return m_fileIndex; }

  /**
   * @brief Get byte offset in file
   */
  uint64_t getOffset() const { return m_offset; }

  /**
   * @brief Get chunk data
   */
  const std::vector<uint8_t> &getData() const { return m_data; }

  /**
   * @brief Get chunk size
   */
  size_t getSize() const { return m_data.size(); }

  /**
   * @brief Check if chunk is valid
   * @return true if has non-empty data
   */
  bool isValid() const { return !m_data.empty(); }

  /**
   * @brief Set chunk data
   * @param fileIndex File index
   * @param offset Byte offset
   * @param data Data pointer
   * @param size Data size
   */
  void setData(uint32_t fileIndex, uint64_t offset, const uint8_t *data, size_t size);

  /**
   * @brief Default chunk size (64 KB)
   */
  static constexpr size_t DEFAULT_CHUNK_SIZE = 65536;

  /**
   * @brief Maximum chunk size (1 MB)
   */
  static constexpr size_t MAX_CHUNK_SIZE = 1048576;

private:
  uint32_t m_fileIndex;       ///< File index in drag operation
  uint64_t m_offset;          ///< Byte offset in file
  std::vector<uint8_t> m_data; ///< Chunk data
};

} // namespace deskflow
