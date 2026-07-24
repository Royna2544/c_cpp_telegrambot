#include <gtest/gtest.h>

#include <cstdint>

#include "FileTransferValidation.hpp"

using namespace tgbot::socket::transfer;

TEST(FileTransferValidation, SizeCap) {
    EXPECT_EQ(kMaxTransferSize, 5ULL * 1024 * 1024 * 1024 / 2);
    EXPECT_TRUE(sizeWithinCap(0));
    EXPECT_TRUE(sizeWithinCap(kMaxTransferSize));
    EXPECT_FALSE(sizeWithinCap(kMaxTransferSize + 1));
}

TEST(FileTransferValidation, CoverageRequiresEveryDeclaredByte) {
    UploadCoverage coverage(10);
    coverage.add(5, 5);
    EXPECT_FALSE(coverage.complete());
    coverage.add(0, 3);
    EXPECT_FALSE(coverage.complete());
    coverage.add(3, 2);
    EXPECT_TRUE(coverage.complete());
}

TEST(FileTransferValidation, CoverageMergesOverlapAndDuplicates) {
    UploadCoverage coverage(8);
    coverage.add(0, 4);
    coverage.add(2, 4);
    coverage.add(0, 4);
    EXPECT_FALSE(coverage.complete());
    coverage.add(6, 2);
    EXPECT_TRUE(coverage.complete());
    EXPECT_EQ(coverage.coveredBytes(), 8U);
}

TEST(FileTransferValidation, ChunkCount) {
    EXPECT_EQ(chunkCount(0), 0);
    EXPECT_EQ(chunkCount(1), 1);
    EXPECT_EQ(chunkCount(kChunkSize), 1);
    EXPECT_EQ(chunkCount(kChunkSize + 1), 2);
    EXPECT_EQ(chunkCount(kChunkSize * 3), 3);
}

TEST(FileTransferValidation, UploadChunkInRange) {
    EXPECT_TRUE(uploadChunkInRange(0, 100, 100));   // exact fit
    EXPECT_TRUE(uploadChunkInRange(50, 50, 100));   // ends exactly at boundary
    EXPECT_TRUE(uploadChunkInRange(100, 0, 100));   // zero-length write at end
    EXPECT_FALSE(uploadChunkInRange(50, 51, 100));  // overruns by one
    EXPECT_FALSE(uploadChunkInRange(-1, 0, 100));   // negative offset
    EXPECT_FALSE(uploadChunkInRange(101, 0, 100));  // offset past end
}

TEST(FileTransferValidation, UploadChunkNoOverflow) {
    // A huge offset must be rejected without overflowing offset + dataSize.
    constexpr auto huge = static_cast<std::int64_t>(1) << 62;
    EXPECT_FALSE(uploadChunkInRange(huge, SIZE_MAX, 100));
}

TEST(FileTransferValidation, DownloadChunkValid) {
    EXPECT_TRUE(downloadChunkValid(0, 3));
    EXPECT_TRUE(downloadChunkValid(2, 3));
    EXPECT_FALSE(downloadChunkValid(3, 3));   // == count is out of range
    EXPECT_FALSE(downloadChunkValid(-1, 3));  // negative
    EXPECT_FALSE(downloadChunkValid(0, 0));   // empty file has no valid chunk
}

TEST(FileTransferValidation, ChunkOffset) {
    EXPECT_EQ(chunkOffset(0), 0U);
    EXPECT_EQ(chunkOffset(1), kChunkSize);
    EXPECT_EQ(chunkOffset(10), kChunkSize * 10);
}
