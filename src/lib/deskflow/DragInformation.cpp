/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/DragInformation.h"

namespace deskflow {

bool DragInformation::addFile(const std::string &filename, uint64_t size, const std::string &mimeType)
{
  if (m_files.size() >= MAX_FILES) {
    return false;
  }

  if (filename.empty() || filename.length() > MAX_FILENAME_LENGTH) {
    return false;
  }

  if (mimeType.length() > MAX_MIMETYPE_LENGTH) {
    return false;
  }

  FileInfo info;
  info.filename = filename;
  info.size = size;
  info.mimeType = mimeType.empty() ? "application/octet-stream" : mimeType;

  m_files.push_back(info);
  return true;
}

uint64_t DragInformation::getTotalSize() const
{
  uint64_t total = 0;
  for (const auto &file : m_files) {
    total += file.size;
  }
  return total;
}

} // namespace deskflow
