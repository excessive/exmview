#include "iqm.hpp"

#include <map>
#include <cstring>
#include <cstdio>
#include <climits>
#include <cassert>
#include <string>
#include <vector>

#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION 2

struct iqmheader {
	char magic[16];
	unsigned int version;
	unsigned int filesize;
	unsigned int flags;
	unsigned int num_text, ofs_text;
	unsigned int num_meshes, ofs_meshes;
	unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
	unsigned int num_triangles, ofs_triangles, ofs_adjacency; // unused
	unsigned int num_joints, ofs_joints;
	unsigned int num_poses, ofs_poses;
	unsigned int num_anims, ofs_anims;
	unsigned int num_frames, num_framechannels, ofs_frames;
	unsigned int ofs_bounds; // unused
	unsigned int num_comment, ofs_comment; // unused
	unsigned int num_extensions, ofs_extensions; // unused
};

struct iqmmesh {
	unsigned int name;
	unsigned int material;
	unsigned int first_vertex, num_vertexes;
	unsigned int first_triangle, num_triangles;
};

enum {
	IQM_POSITION = 0,
	IQM_TEXCOORD = 1,
	IQM_NORMAL = 2,
	IQM_TANGENT = 3,
	IQM_BLENDINDEXES = 4,
	IQM_BLENDWEIGHTS = 5,
	IQM_COLOR = 6,
	IQM_CUSTOM = 0x10
};

enum {
	IQM_BYTE = 0,
	IQM_UBYTE = 1,
	IQM_SHORT = 2,
	IQM_USHORT = 3,
	IQM_INT = 4,
	IQM_UINT = 5,
	IQM_HALF = 6,
	IQM_FLOAT = 7,
	IQM_DOUBLE = 8
};

struct iqmtriangle {
	unsigned int vertex[3];
};

struct iqmjoint {
	unsigned int name;
	int parent;
	float translate[3], rotate[4], scale[3];
};

struct iqmpose {
	int parent;
	unsigned int mask;
	float channeloffset[10];
	float channelscale[10];
};

struct iqmanim {
	unsigned int name;
	unsigned int first_frame, num_frames;
	float framerate;
	unsigned int flags;
};

enum {
	IQM_LOOP = 1 << 0
};

struct iqmvertexarray {
	unsigned int type;
	unsigned int flags;
	unsigned int format;
	unsigned int size;
	unsigned int offset;
};

 struct iqmbounds {
 	float bbmin[3], bbmax[3];
 	float xyradius, radius;
 };

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

static MeshComponent iqm2mc(unsigned int type) {
	switch (type) {
		case IQM_POSITION:     return MC_POSITION;
		case IQM_TEXCOORD:     return MC_TEXCOORD;
		case IQM_NORMAL:       return MC_NORMAL;
		case IQM_TANGENT:      return MC_TANGENT;
		case IQM_BLENDINDEXES: return MC_BONE;
		case IQM_BLENDWEIGHTS: return MC_WEIGHT;
		case IQM_COLOR:        return MC_COLOR;
		default:
			printf("[IQM] Invalid type (potentially corrupt file?)\n");
			assert(false);
			return MC_NONE;
	}
}

static bool should_normalize(unsigned int type) {
	return false
		|| type == IQM_COLOR
		|| type == IQM_BLENDINDEXES
		|| type == IQM_BLENDWEIGHTS
		;
}

static bool check_sanity(const std::string &filename, std::vector<uint8_t> &data, bool &need_intdices) {
	iqmheader *header = (iqmheader *)& data[0];

	if (strncmp(header->magic, IQM_MAGIC, 16) != 0) {
		printf("[IQM] Bad magic for file %s.\n", filename.c_str());
		return false;
	}

	if (header->version != IQM_VERSION) {
		printf("[IQM] Unsupported version.\n");
		return false;
	}

	if (header->num_triangles * 3 > USHRT_MAX) {
		need_intdices = true;
		printf("[IQM] Index limit exceeded, switching to 32-bit...\n");
		if (header->num_triangles * 3 > INT_MAX) {
			printf("[IQM] WAY too many vertices!\n");
			return false;
		}
	}

	assert(header->ofs_vertexarrays + (sizeof(iqmvertexarray) * header->num_vertexarrays) <= data.size());

	return true;
}

struct MeshChunk {
	int offset;
	int num_indices;
	std::string name;
	std::string material;
};

static void read_iqm_chunks(const std::vector<uint8_t> &data, const iqmheader *header, std::vector<MeshChunk> &chunks) {
	chunks.resize(header->num_meshes);
	iqmmesh *meshes = (iqmmesh *)& data[header->ofs_meshes];
	for (unsigned i = 0; i < header->num_meshes; i++) {
		auto *chunk = &chunks[i];
		chunk->offset = meshes[i].first_triangle * 3;
		chunk->num_indices = meshes[i].num_triangles * 3;
		chunk->name = std::string((const char *)& data[header->ofs_text + meshes[i].name]);
		chunk->material = std::string((const char *)& data[header->ofs_text + meshes[i].material]);
	}
}

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
	LayerType type;
	unsigned count;
};

struct MeshData {
	unsigned mask;
	bool need_int_indices;
	std::vector<MeshChunk> chunks;
	std::vector<MeshLayer> layers;
	std::vector<unsigned> indices;
};

namespace fs {
bool read_vector(std::vector<uint8_t> &data, const std::string &filename) {
	return false;
}
}

static bool read_iqm_data(MeshData *md, const std::string &filename) {
	std::vector<uint8_t> data;
	if (fs::read_vector(data, filename)) {
		printf("[IQM] Reading file %s...\n", filename.c_str());
	}
	else {
		printf("[IQM] Couldn't read file %s\n", filename.c_str());
		return false;
	}

	if (!check_sanity(filename, data, md->need_int_indices)) {
		return false;
	}

	/* Read the vertex arrays, so we know what kind of vertex format to expect.
	 * Most of the work here is just some switches. */
	iqmheader *header = (iqmheader *)& data[0];
	iqmvertexarray *vas = (iqmvertexarray *)& data[header->ofs_vertexarrays];
	size_t vsize = 0;
	for (unsigned int i = 0; i < header->num_vertexarrays; i++) {
		iqmvertexarray va = vas[i];
		MeshLayer layer;
		layer.component = iqm2mc(va.type);
		if (va.format == IQM_FLOAT) {
			layer.type = MT_FLOAT;
			switch (va.type) {
				case IQM_TEXCOORD: layer.count = 2; break;
				case IQM_POSITION: layer.count = 3; break;
				case IQM_NORMAL: layer.count = 3; break;
				case IQM_TANGENT: layer.count = 4; break;
				default: assert(false); break;
			}
			layer.data.reserve(header->num_vertexes * layer.count);
			float *fvdata = (float *)&data[va.offset];
			for (size_t i = 0; i < layer.data.capacity(); i++) {
				Rights data;
				data.f = fvdata[i];
				layer.data.push_back(data);
			}
		}
		else if (va.format == IQM_UBYTE) {
			layer.type = MT_INT;
			layer.count = 1;
			layer.data.reserve(header->num_vertexes);
			uint8_t *uvdata = (uint8_t *)& data[va.offset];
			for (size_t i = 0; i < layer.data.size(); i++) {
				Rights data;
				data.u = uvdata[i];
				layer.data.push_back(data);
			}
		}
		else {
			continue;
		}
		md->layers.push_back(layer);
		md->mask |= layer.component;
	}

	// Read the triangle list into an index buffer, convert if needed.
	iqmtriangle *triangles = (iqmtriangle *)& data[header->ofs_triangles];

	for (unsigned int i = 0; i < header->num_triangles; i++) {
		md->indices.push_back(triangles[i].vertex[0]);
		md->indices.push_back(triangles[i].vertex[2]);
		md->indices.push_back(triangles[i].vertex[1]);
	}

	read_iqm_chunks(data, header, md->chunks);

	printf("[IQM] Loaded %s (%d vertices) (Data)\n", filename.c_str(), header->num_vertexes);

	return true;
}
