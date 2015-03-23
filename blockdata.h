#ifndef BLOCKDATA_H
#define BLOCKDATA_H

class BlockData
{
public:
    BlockData();

    BlockData(double avgBZ, int bCount, int lineStart);

    double getAvgBlockSize();

    int getBlockCount();

    int getLineStart();

private:
    double avgBlockSize;

    int blockCount;

    int lineStart;
};

#endif // BLOCKDATA_H
