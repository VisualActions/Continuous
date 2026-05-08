#include "continuous/asset/AssetManager.h"
#include "continuous/core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace cn::asset {

ImportedMesh import_mesh_file(const std::filesystem::path& src) {
    ImportedMesh out;
    Assimp::Importer imp;
    constexpr u32 flags = aiProcess_Triangulate
                        | aiProcess_GenSmoothNormals
                        | aiProcess_CalcTangentSpace
                        | aiProcess_JoinIdenticalVertices
                        | aiProcess_ImproveCacheLocality
                        | aiProcess_GenBoundingBoxes
                        | aiProcess_FlipUVs
                        | aiProcess_ConvertToLeftHanded
                        | aiProcess_LimitBoneWeights;
    const aiScene* scene = imp.ReadFile(src.string(), flags);
    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode) {
        CN_ERROR("asset", "assimp failed for {}: {}", src.string(), imp.GetErrorString());
        return out;
    }

    for (u32 mi = 0; mi < scene->mNumMeshes; ++mi) {
        const aiMesh* m = scene->mMeshes[mi];
        gfx::SubMesh sub;
        sub.first_index = static_cast<u32>(out.indices.size());
        u32 vbase = static_cast<u32>(out.vertices.size());

        for (u32 v = 0; v < m->mNumVertices; ++v) {
            gfx::Vertex vert{};
            vert.position = math::vec3(m->mVertices[v].x, m->mVertices[v].y, m->mVertices[v].z);
            if (m->HasNormals())
                vert.normal = math::vec3(m->mNormals[v].x, m->mNormals[v].y, m->mNormals[v].z);
            if (m->HasTangentsAndBitangents())
                vert.tangent = math::vec4(m->mTangents[v].x, m->mTangents[v].y, m->mTangents[v].z, 1.0f);
            else
                vert.tangent = math::vec4(1, 0, 0, 1);
            if (m->HasTextureCoords(0))
                vert.uv = math::vec2(m->mTextureCoords[0][v].x, m->mTextureCoords[0][v].y);
            if (m->HasVertexColors(0))
                vert.color = math::vec4(m->mColors[0][v].r, m->mColors[0][v].g,
                                        m->mColors[0][v].b, m->mColors[0][v].a);
            else
                vert.color = math::vec4(1, 1, 1, 1);
            out.vertices.push_back(vert);
            out.bounds.expand(vert.position);
        }
        for (u32 fi = 0; fi < m->mNumFaces; ++fi) {
            const auto& f = m->mFaces[fi];
            if (f.mNumIndices != 3) continue;
            out.indices.push_back(vbase + f.mIndices[0]);
            out.indices.push_back(vbase + f.mIndices[1]);
            out.indices.push_back(vbase + f.mIndices[2]);
        }
        sub.index_count = static_cast<u32>(out.indices.size()) - sub.first_index;
        sub.material_id = m->mMaterialIndex;
        sub.bounds.min = math::vec3(m->mAABB.mMin.x, m->mAABB.mMin.y, m->mAABB.mMin.z);
        sub.bounds.max = math::vec3(m->mAABB.mMax.x, m->mAABB.mMax.y, m->mAABB.mMax.z);
        out.submeshes.push_back(sub);
    }
    out.ok = !out.vertices.empty();
    return out;
}

} // namespace cn::asset
