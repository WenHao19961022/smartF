#include "../include/inventory_manager.h"
#include <cassert>
#include <map>

namespace {

FruitInfo MakeFruit(FruitType type, uint8_t x, uint8_t y,
                    uint8_t boxX = 0, uint8_t boxY = 0,
                    uint8_t boxWidth = 0, uint8_t boxHeight = 0) {
    FruitInfo result{};
    result.fruitType = type;
    result.locationX = x;
    result.locationY = y;
    result.freshness = FreshnessLevel::Fresh;
    result.boxX = boxX;
    result.boxY = boxY;
    result.boxWidth = boxWidth;
    result.boxHeight = boxHeight;
    return result;
}

int CountType(const std::vector<TrackedFruit>& stock, FruitType type) {
    int count = 0;
    for (const auto& item : stock) if (item.type == type) ++count;
    return count;
}

} // namespace

int main() {
    std::map<FruitType, int32_t> noWeightEvents;
    std::map<FruitType, int32_t> noDynamicEvents;
    std::map<FruitType, int32_t> averageWeights;

    // A previously registered loose apple remains in inventory when a later
    // pre-filled bag covers its last known position.
    InventoryManager inventory;
    StaticRecognitionResult appleOnly{};
    appleOnly.status = RecognitionStatus::Valid;
    appleOnly.fruitCount = 1;
    appleOnly.fruits[0] = MakeFruit(FruitType::Apple, 110, 120);
    inventory.Reconcile(noWeightEvents, noDynamicEvents, appleOnly, 120, 1);

    StaticRecognitionResult coveredByBag{};
    coveredByBag.status = RecognitionStatus::Valid;
    coveredByBag.fruitCount = 1;
    coveredByBag.fruits[0] = MakeFruit(
        FruitType::PlasticBag, 120, 125, 80, 85, 85, 90);
    inventory.Reconcile(noWeightEvents, noDynamicEvents, coveredByBag, 360, 2);
    auto coveredStock = inventory.GetFlattenedStock(averageWeights);
    assert(CountType(coveredStock, FruitType::Apple) == 1);
    assert(CountType(coveredStock, FruitType::PlasticBag) == 1);

    // Contents of a pre-filled bag were never visible, so no fruit records may
    // be invented from its weight.
    InventoryManager prefilledOnly;
    prefilledOnly.Reconcile(noWeightEvents, noDynamicEvents, coveredByBag, 240, 3);
    auto prefilledStock = prefilledOnly.GetFlattenedStock(averageWeights);
    assert(CountType(prefilledStock, FruitType::PlasticBag) == 1);
    assert(CountType(prefilledStock, FruitType::Apple) == 0);
    assert(CountType(prefilledStock, FruitType::Banana) == 0);
    assert(CountType(prefilledStock, FruitType::Orange) == 0);
    return 0;
}
