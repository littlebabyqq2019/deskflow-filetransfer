/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 - 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "deskflow/FileChunk.h"

#include <algorithm>

namespace deskflow {

FileChunk::FileChunk(uint32_t fileIndex, uint64_t offset, const uint8_t *data, size_t size)
    : m_fileIndex(fileIndex), m_offset(offset)
{
  if (data && size > 0 && size <= MAX_CHUNK_SIZE) {
    m_data.assign(data, data + size);
  }
}

void FileChunk::setData(uint32_t fileIndex, uint64_t offset, const uint8_t *data, size_t size)
{
  m_fileIndex = fileIndex;
  m_offset = offset;
  m_data.clear();

  if (data && size > 0 && size <= MAX_CHUNK_SIZE) {
    m_data.assign(data, data + size);
  }
}

} // namespace deskflow
