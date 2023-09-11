#pragma once
namespace VKGeometry {

struct StaticMesh;
struct SkeletalMesh;
struct StaticModel;
struct SkeletalModel;
struct GPUSkinnedModel;
struct GeometryHandler;
struct UniformMatricesHandler;

void make_static_model(StaticModel* model, StaticMesh& mesh);
void make_skeletal_model(SkeletalModel* model, SkeletalMesh& mesh);

}