/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace deskflow {

/**
 * @brief File metadata for drag-and-drop file transfer
 *
 * Contains information about a single file being dragged:
 * - Filename (UTF-8, up to 260 chars for Windows MAX_PATH compatibility)
 * - File size in bytes
 * - MIME type for content identification
 *
 * Used in kMsgDDragInfo (DDRG) messages during drag initiation.
 *
 * @since Protocol version 1.9
 */
struct FileInfo
{
  std::string filename; ///< Original filename (basename only, no path)
  uint64_t size;        ///< File size in bytes (0 for directories/symlinks)
  std::string mimeType; ///< MIME type (e.g., "text/plain", "image/png")
};

/**
 * @brief Drag-and-drop operation metadata
 *
 * Transmitted via kMsgDDragInfo (DDRG) when user initiates a file drag.
 * Contains list of files and cursor position at drag start.
 *
 * **Wire Format** (kMsgDDragInfo):
 * - File count (4 bytes, uint32_t)
 * - For each file:
 *   - Filename length (4 bytes) + UTF-8 string
 *   - File size (8 bytes, uint64_t)
 *   - MIME type length (4 bytes) + UTF-8 string
 *
 * **Limits**:
 * - Max 256 files per drag operation
 * - Max filename length: 260 bytes (UTF-8)
 * - Max MIME type length: 128 bytes
 *
 * @see kMsgDDragInfo, FileInfo
 * @since Protocol version 1.9
 */
class DragInformation
{
public:
  DragInformation() = default;

  /**
   * @brief Add a file to the drag operation
   * @param filename File basename (no directory path)
   * @param size File size in bytes
   * @param mimeType MIME type string
   * @return true if added successfully, false if limit exceeded
   */
  bool addFile(const std::string &filename, uint64_t size, const std::string &mimeType);

  /**
   * @brief Get list of files in this drag operation
   */
  const std::vector<FileInfo> &getFiles() const { return m_files; }

  /**
   * @brief Get total number of files
   */
  size_t getFileCount() const { return m_files.size(); }

  /**
   * @brief Get total size of all files
   */
  uint64_t getTotalSize() const;

  /**
   * @brief Clear all files
   */
  void clear() { m_files.clear(); }

  /**
   * @brief Check if drag operation is valid
   * @return true if has at least one file
   */
  bool isValid() const { return !m_files.empty(); }

  /**
   * @brief Maximum files per drag operation
   */
  static constexpr size_t MAX_FILES = 256;

  /**
   * @brief Maximum filename length (bytes, UTF-8)
   */
  static constexpr size_t MAX_FILENAME_LENGTH = 260;

  /**
   * @brief Maximum MIME type length (bytes)
   */
  static constexpr size_t MAX_MIMETYPE_LENGTH = 128;

private:
  std::vector<FileInfo> m_files;
};

} // namespace deskflow
