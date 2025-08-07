#include <iostream>
#include <cassert>
#include "core/mixed_voxel.hpp"  // Now the primary version
#include "core/material_table.hpp"

using namespace octree;
using namespace core;

// Test the new MixedVoxel structure with material IDs
void test_mixed_voxel_structure() {
    std::cout << "Testing new MixedVoxel structure..." << std::endl;
    
    // Verify size is still 8 bytes
    static_assert(sizeof(MixedVoxel) == 8, "MixedVoxel must be exactly 8 bytes");
    std::cout << "  ✓ MixedVoxel is 8 bytes" << std::endl;
}

void test_material_id_storage() {
    std::cout << "Testing material ID storage..." << std::endl;
    
    MixedVoxel voxel;
    
    // Set 4 different materials
    voxel.setMaterials(
        MaterialID::Rock, 128,    // 50% rock
        MaterialID::Water, 64,     // 25% water
        MaterialID::Sand, 64,      // 25% sand
        MaterialID::Vacuum, 0      // 0% vacuum (unused slot)
    );
    
    // Verify we can retrieve them
    assert(voxel.getMaterialID(0) == MaterialID::Rock);
    assert(voxel.getMaterialAmount(0) == 128);
    
    assert(voxel.getMaterialID(1) == MaterialID::Water);
    assert(voxel.getMaterialAmount(1) == 64);
    
    assert(voxel.getMaterialID(2) == MaterialID::Sand);
    assert(voxel.getMaterialAmount(2) == 64);
    
    assert(voxel.getMaterialID(3) == MaterialID::Vacuum);
    assert(voxel.getMaterialAmount(3) == 0);
    
    std::cout << "  ✓ Can store and retrieve 4 material IDs with amounts" << std::endl;
}

void test_dominant_material() {
    std::cout << "Testing dominant material detection..." << std::endl;
    
    MixedVoxel voxel;
    
    // Test case 1: Single material
    voxel.setMaterials(
        MaterialID::Rock, 255,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0
    );
    assert(voxel.getDominantMaterialID() == MaterialID::Rock);
    std::cout << "  ✓ Single material detected as dominant" << std::endl;
    
    // Test case 2: Mixed materials
    voxel.setMaterials(
        MaterialID::Water, 100,
        MaterialID::Sand, 150,   // Sand is dominant
        MaterialID::Rock, 50,
        MaterialID::Vacuum, 0
    );
    assert(voxel.getDominantMaterialID() == MaterialID::Sand);
    std::cout << "  ✓ Correct dominant material in mix" << std::endl;
    
    // Test case 3: Empty voxel
    voxel.setMaterials(
        MaterialID::Vacuum, 255,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0
    );
    assert(voxel.getDominantMaterialID() == MaterialID::Vacuum);
    assert(voxel.isEmpty());
    std::cout << "  ✓ Empty voxel detected" << std::endl;
}

void test_color_calculation() {
    std::cout << "Testing color calculation..." << std::endl;
    
    MixedVoxel voxel;
    auto& matTable = MaterialTable::getInstance();
    
    // Test pure water
    voxel.setMaterials(
        MaterialID::Water, 255,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0
    );
    
    glm::vec3 color = voxel.getColor();
    glm::vec3 waterColor = matTable.getColor(MaterialID::Water);
    assert(glm::length(color - waterColor) < 0.01f);
    std::cout << "  ✓ Pure water has correct color" << std::endl;
    
    // Test 50/50 rock/grass mix
    voxel.setMaterials(
        MaterialID::Rock, 128,
        MaterialID::Grass, 128,
        MaterialID::Vacuum, 0,
        MaterialID::Vacuum, 0
    );
    
    color = voxel.getColor();
    glm::vec3 expectedColor = (matTable.getColor(MaterialID::Rock) + 
                                matTable.getColor(MaterialID::Grass)) * 0.5f;
    assert(glm::length(color - expectedColor) < 0.01f);
    std::cout << "  ✓ 50/50 mix has blended color" << std::endl;
}

void test_factory_methods() {
    std::cout << "Testing factory methods..." << std::endl;
    
    // Test createPure
    MixedVoxel pure = MixedVoxel::createPure(MaterialID::Lava);
    assert(pure.getMaterialID(0) == MaterialID::Lava);
    assert(pure.getMaterialAmount(0) == 255);
    assert(pure.getMaterialAmount(1) == 0);
    std::cout << "  ✓ createPure creates single material voxel" << std::endl;
    
    // Test createMix (2 materials)
    MixedVoxel mix = MixedVoxel::createMix(
        MaterialID::Sand, 200,
        MaterialID::Water, 55
    );
    assert(mix.getMaterialID(0) == MaterialID::Sand);
    assert(mix.getMaterialAmount(0) == 200);
    assert(mix.getMaterialID(1) == MaterialID::Water);
    assert(mix.getMaterialAmount(1) == 55);
    std::cout << "  ✓ createMix creates two-material voxel" << std::endl;
    
    // Test createEmpty
    MixedVoxel empty = MixedVoxel::createEmpty();
    assert(empty.isEmpty());
    assert(empty.getDominantMaterialID() == MaterialID::Vacuum);
    std::cout << "  ✓ createEmpty creates vacuum voxel" << std::endl;
}

void test_should_render() {
    std::cout << "Testing render decision..." << std::endl;
    
    // Empty voxel should not render
    MixedVoxel empty = MixedVoxel::createEmpty();
    assert(!empty.shouldRender());
    std::cout << "  ✓ Empty voxel marked as non-renderable" << std::endl;
    
    // Pure air should not render (unless we want atmosphere)
    MixedVoxel air = MixedVoxel::createPure(MaterialID::Air);
    assert(!air.shouldRender());  // Assuming we don't render pure air
    std::cout << "  ✓ Pure air marked as non-renderable" << std::endl;
    
    // Rock should render
    MixedVoxel rock = MixedVoxel::createPure(MaterialID::Rock);
    assert(rock.shouldRender());
    std::cout << "  ✓ Rock marked as renderable" << std::endl;
    
    // Mostly air with some rock should render
    MixedVoxel mostly_air = MixedVoxel::createMix(
        MaterialID::Air, 200,
        MaterialID::Rock, 55
    );
    assert(mostly_air.shouldRender());
    std::cout << "  ✓ Mostly air with some solid marked as renderable" << std::endl;
}

void test_averaging() {
    std::cout << "Testing voxel averaging..." << std::endl;
    
    // Create 8 child voxels with different materials
    MixedVoxel children[8];
    children[0] = MixedVoxel::createPure(MaterialID::Rock);
    children[1] = MixedVoxel::createPure(MaterialID::Rock);
    children[2] = MixedVoxel::createPure(MaterialID::Water);
    children[3] = MixedVoxel::createPure(MaterialID::Water);
    children[4] = MixedVoxel::createPure(MaterialID::Sand);
    children[5] = MixedVoxel::createPure(MaterialID::Sand);
    children[6] = MixedVoxel::createPure(MaterialID::Grass);
    children[7] = MixedVoxel::createPure(MaterialID::Air);
    
    // Average them
    MixedVoxel parent = MixedVoxel::average(children);
    
    // Parent should have all 5 materials, but only 4 can be stored
    // Should keep the 4 most abundant (Rock, Water, Sand, Grass - not Air)
    bool hasRock = false, hasWater = false, hasSand = false, hasGrass = false;
    
    for (int i = 0; i < 4; i++) {
        MaterialID id = parent.getMaterialID(i);
        if (id == MaterialID::Rock) hasRock = true;
        if (id == MaterialID::Water) hasWater = true;
        if (id == MaterialID::Sand) hasSand = true;
        if (id == MaterialID::Grass) hasGrass = true;
    }
    
    assert(hasRock && hasWater && hasSand);
    std::cout << "  ✓ Averaging preserves multiple materials" << std::endl;
    
    // Test that amounts are reasonable (each should be ~64 since 2/8 children)
    for (int i = 0; i < 4; i++) {
        uint8_t amount = parent.getMaterialAmount(i);
        if (parent.getMaterialID(i) != MaterialID::Vacuum) {
            assert(amount > 30 && amount < 100);  // Roughly 2/8 * 255 = 64
        }
    }
    std::cout << "  ✓ Averaged amounts are proportional" << std::endl;
}

void test_backwards_compatibility() {
    std::cout << "Testing backwards compatibility..." << std::endl;
    
    // The new system should be able to represent the old fixed materials
    // Old: rock, water, air, sediment
    
    MixedVoxel oldStyle;
    oldStyle.setMaterials(
        MaterialID::Rock, 100,
        MaterialID::Water, 50,
        MaterialID::Air, 100,
        MaterialID::Sand, 5  // Using sand as sediment
    );
    
    // Should still work with similar logic
    assert(oldStyle.getMaterialID(0) == MaterialID::Rock);
    assert(oldStyle.getMaterialID(1) == MaterialID::Water);
    assert(oldStyle.getMaterialID(2) == MaterialID::Air);
    assert(oldStyle.getMaterialID(3) == MaterialID::Sand);
    
    std::cout << "  ✓ Can represent old material system" << std::endl;
}

int main() {
    std::cout << "\n=== Mixed Voxel V2 Test Suite ===\n" << std::endl;
    
    test_mixed_voxel_structure();
    test_material_id_storage();
    test_dominant_material();
    test_color_calculation();
    test_factory_methods();
    test_should_render();
    test_averaging();
    test_backwards_compatibility();
    
    std::cout << "\n=== All Mixed Voxel V2 Tests Passed ===\n" << std::endl;
    return 0;
}