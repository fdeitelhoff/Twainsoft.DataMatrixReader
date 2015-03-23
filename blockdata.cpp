#include "blockdata.h"

BlockData::BlockData()
{
}

BlockData::BlockData(double avgBZ, int bCount, int start)
{
    this->avgBlockSize = avgBZ;
    this->blockCount = bCount;
    this->lineStart = start;
}

double BlockData::getAvgBlockSize()
{
    return this->avgBlockSize;
}

int BlockData::getBlockCount()
{
    return this->blockCount;
}

int BlockData::getLineStart()
{
    return this->lineStart;
}
