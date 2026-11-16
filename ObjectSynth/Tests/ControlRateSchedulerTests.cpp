
#include "../Source/DSP/ControlRateScheduler.h"

#include <cassert>
#include <vector>
#include <iostream>

std::vector<int> getUpdatePositions(
    const std::vector<int>& blockSizes)
{
    ControlRateScheduler scheduler;

    std::vector<int> positions;
    int absoluteSample = 0;

    for (int blockSize : blockSizes)
    {
        int offset = 0;

        while (offset < blockSize)
        {
            if (scheduler.isUpdateDue())
                positions.push_back(absoluteSample + offset);

            const int chunk =
                scheduler.nextChunkSize(blockSize - offset);

            assert(chunk > 0 && chunk <= 32);

            scheduler.advance(chunk);
            offset += chunk;
        }

        absoluteSample += blockSize;
    }

    return positions;
}

int main()
{
    std::vector<int> expected;

    for (int i = 0; i < 520; i += 32)
        expected.push_back(i);

    // Same audio length, different buffer partitioning.
    assert(getUpdatePositions({520}) == expected);

    assert(getUpdatePositions({128, 256, 136}) == expected);

    assert(getUpdatePositions({40, 480}) == expected);

    assert(getUpdatePositions({1, 7, 63, 125, 324}) == expected);

    ControlRateScheduler scheduler;

    assert(scheduler.isUpdateDue());

    scheduler.advance(11);

    assert(!scheduler.isUpdateDue());
    assert(scheduler.nextChunkSize(300) == 21);

    scheduler.reset();

    assert(scheduler.isUpdateDue());

    std::cout << "Control Rate Tests: PASSED\n";
}
