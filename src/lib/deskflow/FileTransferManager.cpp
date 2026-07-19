/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileTransferManager.h"

#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace deskflow {

FileTransferManager::FileTransferManager()
    : m_transferActive(false), m_totalExpectedBytes(0), m_totalReceivedBytes(0)
{
  // Set default drop directory
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  if (SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, path) == S_OK) {
    char mbPath[MAX_PATH * 3];
    WideCharToMultiByte(CP_UTF8, 0, path, -1, mbPath, sizeof(mbPath), NULL, NULL);
    m_dropDirectory = std::string(mbPath) + "\\Downloads\\DeskflowDrop";
  }
#else
  const char *home = getenv("HOME");
  if (!home) {
    struct passwd *pw = getpwuid(getuid());
    if (pw)
      home = pw->pw_dir;
  }
  if (home) {
    m_dropDirectory = std::string(home) + "/Downloads/DeskflowDrop";
  }
#endif
}

FileTransferManager::~FileTransferManager()
{
  cleanup();
}

bool FileTransferManager::createDropDirectory()
{
  try {
    std::filesystem::create_directories(m_dropDirectory);
    return true;
  } catch (...) {
    return false;
  }
}

std::string FileTransferManager::makeUniqueFilename(const std::string &basename)
{
  std::filesystem::path basePath(m_dropDirectory);
  basePath /= basename;

  if (!std::filesystem::exists(basePath)) {
    return basePath.string();
  }

  // Add (1), (2), etc. until we find a unique name
  std::filesystem::path stem = basePath.stem();
  std::filesystem::path ext = basePath.extension();

  for (int i = 1; i < 1000; ++i) {
    std::filesystem::path newPath = basePath.parent_path() /
      (stem.string() + " (" + std::to_string(i) + ")" + ext.string());
    if (!std::filesystem::exists(newPath)) {
      return newPath.string();
    }
  }

  return basePath.string();
}

bool FileTransferManager::onDragInfo(const DragInformation &dragInfo)
{
  if (m_transferActive) {
    cleanup();
  }

  if (!dragInfo.isValid()) {
    return false;
  }

  if (!createDropDirectory()) {
    return false;
  }

  m_currentDrag = dragInfo;
  m_transferActive = true;
  m_totalExpectedBytes = dragInfo.getTotalSize();
  m_totalReceivedBytes = 0;
  m_fileStates.clear();

  return true;
}

bool FileTransferManager::openFile(uint32_t fileIndex)
{
  if (fileIndex >= m_currentDrag.getFileCount()) {
    return false;
  }

  auto it = m_fileStates.find(fileIndex);
  if (it != m_fileStates.end() && it->second.fileHandle) {
    return true; // Already open
  }

  const FileInfo &info = m_currentDrag.getFiles()[fileIndex];
  std::string fullPath = makeUniqueFilename(info.filename);

  FILE *f = fopen(fullPath.c_str(), "wb");
  if (!f) {
    return false;
  }

  FileState state;
  state.filename = info.filename;
  state.fullPath = fullPath;
  state.expectedSize = info.size;
  state.receivedBytes = 0;
  state.fileHandle = f;

  m_fileStates[fileIndex] = state;
  return true;
}

bool FileTransferManager::onFileChunk(const FileChunk &chunk)
{
  if (!m_transferActive || !chunk.isValid()) {
    return false;
  }

  uint32_t fileIndex = chunk.getFileIndex();
  if (fileIndex >= m_currentDrag.getFileCount()) {
    return false;
  }

  if (!openFile(fileIndex)) {
    return false;
  }

  FileState &state = m_fileStates[fileIndex];
  if (!state.fileHandle) {
    return false;
  }

  // Write chunk data
  size_t written = fwrite(chunk.getData().data(), 1, chunk.getSize(), state.fileHandle);
  if (written != chunk.getSize()) {
    return false;
  }

  state.receivedBytes += written;
  m_totalReceivedBytes += written;

  return true;
}

bool FileTransferManager::onTransferDone()
{
  if (!m_transferActive) {
    return false;
  }

  closeAllFiles();

  // Verify all files received completely
  for (const auto &pair : m_fileStates) {
    if (pair.second.receivedBytes != pair.second.expectedSize) {
      cleanup();
      return false;
    }
  }

  m_transferActive = false;
  return true;
}

void FileTransferManager::onTransferError(const std::string &errorMessage)
{
  cleanup();
}

void FileTransferManager::closeAllFiles()
{
  for (auto &pair : m_fileStates) {
    if (pair.second.fileHandle) {
      fclose(pair.second.fileHandle);
      pair.second.fileHandle = nullptr;
    }
  }
}

void FileTransferManager::cleanup()
{
  closeAllFiles();
  m_fileStates.clear();
  m_currentDrag.clear();
  m_transferActive = false;
  m_totalExpectedBytes = 0;
  m_totalReceivedBytes = 0;
}

float FileTransferManager::getProgress() const
{
  if (m_totalExpectedBytes == 0) {
    return 0.0f;
  }
  return static_cast<float>(m_totalReceivedBytes) / static_cast<float>(m_totalExpectedBytes);
}

void FileTransferManager::setDropDirectory(const std::string &dir)
{
  if (!dir.empty()) {
    m_dropDirectory = dir;
  }
}

} // namespace deskflow
