#pragma once

#include <vector>
#include <string>

enum MeshComponent {
	MC_NONE = 0,
	MC_POSITION = 1 << 0,
	MC_NORMAL = 1 << 1,
	MC_TEXCOORD = 1 << 2,
	MC_TANGENT = 1 << 3,
	MC_BONE = 1 << 4,
	MC_WEIGHT = 1 << 5,
	MC_COLOR = 1 << 6
};

union Rights {
	unsigned u;
	float f;
};

enum LayerType {
	MT_INT,
	MT_FLOAT
};

struct MeshLayer {
	MeshComponent component;
	std::vector<Rights> data;
	size_t bytes;
	LayerType type;
	unsigned count;
	bool should_normalize;
};

struct MeshChunk {
	unsigned offset;
	unsigned num_indices;
	std::string name;
	std::string material;
};

struct MeshData {
	unsigned mask;
	bool need_int_indices;
	std::vector<MeshChunk> chunks;
	std::vector<MeshLayer> layers;
	std::vector<unsigned> indices;
	size_t indices_bytes;
};

bool iqm_read_data(MeshData *md, const std::string &filename, bool fill_colors);
