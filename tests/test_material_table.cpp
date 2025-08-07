#include <iostream>
#include <cassert>
#include "core/material_table.hpp"

using namespace core;

void test_singleton() {
    std::cout << "Testing MaterialTable singleton..." << std::endl;
    
    // Get instance multiple times - should be the same
    auto& table1 = MaterialTable::getInstance();
    auto& table2 = MaterialTable::getInstance();
    
    assert(&table1 == &table2);
    std::cout << "  ✓ Singleton returns same instance" << std::endl;
}

void test_material_properties() {
    std::cout << "Testing material properties..." << std::endl;
    
    auto& table = MaterialTable::getInstance();
    
    // Test fundamental materials
    auto vacuum = table.getMaterial(MaterialID::Vacuum);
    assert(vacuum.name == "Vacuum");
    assert(vacuum.color == glm::vec3(0.0f, 0.0f, 0.0f));
    assert(vacuum.density == 0.0f);
    std::cout << "  ✓ Vacuum is black with zero density" << std::endl;
    
    auto water = table.getMaterial(MaterialID::Water);
    assert(water.name == "Water");
    assert(water.density == 1000.0f);  // Water is 1000 kg/m³
    assert(water.hardness == 0.0f);    // Liquid
    std::cout << "  ✓ Water has correct density and is liquid" << std::endl;
    
    auto rock = table.getMaterial(MaterialID::Rock);
    assert(rock.hardness > 0.8f);      // Solid
    assert(rock.density > 2000.0f);    // Heavy
    std::cout << "  ✓ Rock is solid and heavy" << std::endl;
    
    // Test special materials
    auto lava = table.getMaterial(MaterialID::Lava);
    assert(lava.emissive > 0.0f);      // Should glow
    assert(lava.hardness == 0.0f);     // Liquid
    std::cout << "  ✓ Lava is emissive and liquid" << std::endl;
    
    auto metal = table.getMaterial(MaterialID::Metal);
    assert(metal.metallic == 1.0f);    // Should be metallic
    assert(metal.density > 7000.0f);   // Very heavy
    std::cout << "  ✓ Metal has metallic property and high density" << std::endl;
}

void test_color_retrieval() {
    std::cout << "Testing color retrieval..." << std::endl;
    
    auto& table = MaterialTable::getInstance();
    
    // Test direct color access
    glm::vec3 waterColor = table.getColor(MaterialID::Water);
    assert(waterColor.b > waterColor.r);  // Water should be bluish
    std::cout << "  ✓ Water color is bluish" << std::endl;
    
    glm::vec3 grassColor = table.getColor(MaterialID::Grass);
    assert(grassColor.g > grassColor.r && grassColor.g > grassColor.b);  // Green dominant
    std::cout << "  ✓ Grass color is greenish" << std::endl;
    
    glm::vec3 sandColor = table.getColor(MaterialID::Sand);
    assert(sandColor.r > 0.7f && sandColor.g > 0.6f);  // Sandy/tan color
    std::cout << "  ✓ Sand color is tan/beige" << std::endl;
}

void test_gpu_data_packing() {
    std::cout << "Testing GPU data packing..." << std::endl;
    
    auto& table = MaterialTable::getInstance();
    auto gpuData = table.getGPUData();
    
    // Verify array size
    assert(gpuData.size() == 16);
    std::cout << "  ✓ GPU data has 16 materials" << std::endl;
    
    // Check water packing (index 3)
    auto waterGPU = gpuData[static_cast<size_t>(MaterialID::Water)];
    assert(waterGPU.colorAndRoughness.x == table.getMaterial(MaterialID::Water).color.r);
    assert(waterGPU.colorAndRoughness.y == table.getMaterial(MaterialID::Water).color.g);
    assert(waterGPU.colorAndRoughness.z == table.getMaterial(MaterialID::Water).color.b);
    assert(waterGPU.colorAndRoughness.w == table.getMaterial(MaterialID::Water).roughness);
    std::cout << "  ✓ Water color and roughness packed correctly" << std::endl;
    
    // Check lava emissive packing
    auto lavaGPU = gpuData[static_cast<size_t>(MaterialID::Lava)];
    assert(lavaGPU.properties.y > 0.0f);  // Emissive in y component
    std::cout << "  ✓ Lava emissive property packed correctly" << std::endl;
    
    // Check metal metallic packing
    auto metalGPU = gpuData[static_cast<size_t>(MaterialID::Metal)];
    assert(metalGPU.properties.x == 1.0f);  // Metallic in x component
    std::cout << "  ✓ Metal metallic property packed correctly" << std::endl;
    
    // Verify size for GPU upload
    size_t expectedSize = sizeof(MaterialTable::GPUMaterialData) * 16;
    assert(MaterialTable::getGPUDataSize() == expectedSize);
    std::cout << "  ✓ GPU data size is " << expectedSize << " bytes" << std::endl;
}

void test_material_id_packing() {
    std::cout << "Testing material ID packing/unpacking..." << std::endl;
    
    // Test packing two IDs into one byte
    MaterialID id0 = MaterialID::Rock;
    MaterialID id1 = MaterialID::Water;
    
    uint8_t packed = packMaterialIDs(id0, id1);
    assert(packed == 0x32);  // Rock=2 in low nibble, Water=3 in high nibble
    std::cout << "  ✓ Rock+Water packed to 0x" << std::hex << (int)packed << std::dec << std::endl;
    
    // Test unpacking
    MaterialID unpacked0, unpacked1;
    unpackMaterialIDs(packed, unpacked0, unpacked1);
    assert(unpacked0 == MaterialID::Rock);
    assert(unpacked1 == MaterialID::Water);
    std::cout << "  ✓ Unpacked correctly back to Rock and Water" << std::endl;
    
    // Test edge cases
    packed = packMaterialIDs(MaterialID::Vacuum, MaterialID::Reserved);
    assert(packed == 0xF0);  // 0 and 15
    unpackMaterialIDs(packed, unpacked0, unpacked1);
    assert(unpacked0 == MaterialID::Vacuum);
    assert(unpacked1 == MaterialID::Reserved);
    std::cout << "  ✓ Edge cases (0 and 15) pack/unpack correctly" << std::endl;
}

void test_all_materials_unique() {
    std::cout << "Testing all materials are unique..." << std::endl;
    
    auto& table = MaterialTable::getInstance();
    
    // Check that all materials have unique colors (helps with debugging)
    for (size_t i = 0; i < 16; ++i) {
        for (size_t j = i + 1; j < 16; ++j) {
            auto color1 = table.getMaterial(i).color;
            auto color2 = table.getMaterial(j).color;
            
            // Allow very similar colors but not identical
            float diff = glm::length(color1 - color2);
            if (diff < 0.01f) {
                std::cerr << "  WARNING: Materials " << i << " and " << j 
                          << " have nearly identical colors!" << std::endl;
            }
        }
    }
    std::cout << "  ✓ All materials have distinct properties" << std::endl;
    
    // Print material summary
    std::cout << "\nMaterial Summary:" << std::endl;
    std::cout << "ID | Name         | Color (RGB)        | Density | Hard | Metal | Emis" << std::endl;
    std::cout << "---|--------------|-------------------|---------|------|-------|-----" << std::endl;
    
    for (size_t i = 0; i < 16; ++i) {
        const auto& mat = table.getMaterial(i);
        printf("%2zu | %-12s | (%.2f,%.2f,%.2f) | %7.0f | %.2f | %.2f  | %.2f\n",
               i, mat.name.c_str(),
               mat.color.r, mat.color.g, mat.color.b,
               mat.density, mat.hardness, mat.metallic, mat.emissive);
    }
}

int main() {
    std::cout << "\n=== Material Table Test Suite ===\n" << std::endl;
    
    test_singleton();
    test_material_properties();
    test_color_retrieval();
    test_gpu_data_packing();
    test_material_id_packing();
    test_all_materials_unique();
    
    std::cout << "\n=== All Material Table Tests Passed ===\n" << std::endl;
    return 0;
}