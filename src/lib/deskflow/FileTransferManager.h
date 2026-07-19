/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/DragInformation.h"
#include "deskflow/FileChunk.h"

#include <cstdio>
#include <map>
#include <memory>
#include <string>

namespace deskflow {

/**
 * @brief Server-side file transfer receiver and assembler
 *
 * Manages incoming file transfers:
 * - Receives DragInformation (DDRG) with file metadata
 * - Receives FileChunks (DFTR) and writes to disk
 * - Tracks transfer progress and handles completion (DFDN)
 * - Cleans up on error (DFER) or cancellation
 *
 * **State Machine**:
 * 1. Idle → onDragInfo() → Receiving
 * 2. Receiving → onFileChunk() → write chunk, track progress
 * 3. Receiving → onTransferDone() → Completed
 * 4. Any → onTransferError() → Cleanup
 *
 * **File Storage**:
 * - Files written to platform-specific drop directory
 * - Windows: %USERPROFILE%\Downloads\DeskflowDrop
 * - Linux/macOS: ~/Downloads/DeskflowDrop
 * - Filename collision: append (1), (2), etc.
 *
 * @see DragInformation, FileChunk
 * @since Protocol version 1.9
 */
class FileTransferManager
{
public:
  FileTransferManager();
  ~FileTransferManager();

  /**
   * @brief Handle incoming drag info (DDRG)
   * @param dragInfo File metadata
   * @return true if transfer started successfully
   */
  bool onDragInfo(const DragInformation &dragInfo);

  /**
   * @brief Handle incoming file chunk (DFTR)
   * @param chunk File data chunk
   * @return true if chunk written successfully
   */
  bool onFileChunk(const FileChunk &chunk);

  /**
   * @brief Handle transfer completion (DFDN)
   * @return true if all files received successfully
   */
  bool onTransferDone();

  /**
   * @brief Handle transfer error (DFER)
   * @param errorMessage Error description
   */
  void onTransferError(const std::string &errorMessage);

  /**
   * @brief Check if transfer is active
   */
  bool isTransferActive() const { return m_transferActive; }

  /**
   * @brief Get current transfer progress (0.0 - 1.0)
   */
  float getProgress() const;

  /**
   * @brief Get target directory for received files
   */
  std::string getDropDirectory() const { return m_dropDirectory; }

  /**
   * @brief Set target directory for received files
   * @param dir Absolute path (created if doesn't exist)
   */
  void setDropDirectory(const std::string &dir);

private:
  struct FileState
  {
    std::string filename;
    std::string fullPath;
    uint64_t expectedSize;
    uint64_t receivedBytes;
    FILE *fileHandle;
  };

  bool createDropDirectory();
  std::string makeUniqueFilename(const std::string &basename);
  bool openFile(uint32_t fileIndex);
  void closeAllFiles();
  void cleanup();

  bool m_transferActive;
  std::string m_dropDirectory;
  DragInformation m_currentDrag;
  std::map<uint32_t, FileState> m_fileStates;
  uint64_t m_totalExpectedBytes;
  uint64_t m_totalReceivedBytes;
};

} // namespace deskflow
