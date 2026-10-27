#pragma once
#include "Policy.h"
#include <unordered_map>
#include <map>
#include <cmath>
#include <functional>
#include <cstring>
namespace PS::Buildings {
class BuildingGlow {
 struct Shape {WeakObject mesh;Transform local;};
 struct Batch {WeakObject component;std::vector<Transform> previous;};
 WeakObject actor,coordinator,material;float elapsed=0;
 std::unordered_map<UObject*,std::vector<Shape>> shapes;
 std::unordered_map<UObject*,Batch> batches;
 static Transform Identity(){Transform t{};t.rotation[3]=1;t.scale={1,1,1};return t;}
 static UObject* ObjectField(UStruct* type,void* owner,const wchar_t* name){
  auto* p=CastField<FObjectPropertyBase>(Field(type,name));if(!p)throw std::runtime_error("Glow object field differs");return p->GetObjectPropertyValue(p->ContainerPtrToValuePtr<void>(owner));
 }
 static void EachStruct(FArrayProperty* p,void* owner,const std::function<void(UScriptStruct*,void*)>& visit){
  auto* inner=p?CastField<FStructProperty>(p->GetInner()):nullptr;if(!inner)throw std::runtime_error("Glow geometry array differs");
  auto* array=p->ContainerPtrToValuePtr<FScriptArray>(owner);if(array->Num()<0||array->Num()>100000)throw std::runtime_error("Invalid geometry count");
  for(int i=0;i<array->Num();++i)visit(inner->GetStruct(),static_cast<char*>(array->GetData())+i*inner->GetElementSize());
 }
 std::vector<Shape>& Geometry(UObject* data){
  if(auto found=shapes.find(data);found!=shapes.end())return found->second;
  Call load(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"LoadAsset_Blocking");load.Copy(L"Asset",Value(data->GetClassPrivate(),data,L"DerivedData"));load.Invoke();auto* derived=load.Result();
  if(!derived)throw std::runtime_error("Building render definition missing");
  auto* rep=CastField<FStructProperty>(Field(derived->GetClassPrivate(),L"EntityRepresentation"));auto* collection=CastField<FStructProperty>(Field(rep->GetStruct(),L"EntityCollection"));
  auto* entities=CastField<FArrayProperty>(Field(collection->GetStruct(),L"Entities"));std::vector<Shape> result;
  EachStruct(entities,collection->ContainerPtrToValuePtr<void>(rep->ContainerPtrToValuePtr<void>(derived)),[&](UScriptStruct* type,void* entity){
   auto* fragments=CastField<FArrayProperty>(Field(type,L"Fragments"));if(!fragments||fragments->GetInner()->GetElementSize()!=sizeof(Instanced))throw std::runtime_error("Building render fragments differ");
   auto* values=fragments->ContainerPtrToValuePtr<FScriptArray>(entity);auto* list=static_cast<Instanced*>(values->GetData());Transform local=Identity();UObject* mesh=nullptr;bool visible=true;
   for(int i=0;i<values->Num();++i)if(list[i].type&&list[i].data){
    if(list[i].type->GetName()==L"JgxStaticMeshFragment")mesh=ObjectField(list[i].type,list[i].data,L"StaticMesh");
    if(list[i].type->GetName()==L"JgxPrimitiveFragment")visible=Boolean(list[i].type,list[i].data,L"bVisible")&&!Boolean(list[i].type,list[i].data,L"bHiddenInGame");
    if(list[i].type->GetName()==L"JgxTransformFragment"){
     auto* p=Field(list[i].type,L"Transform");if(p->GetSize()!=sizeof(Transform))throw std::runtime_error("Building render transform differs");local=*p->ContainerPtrToValuePtr<Transform>(list[i].data);
    }
   }
   if(mesh&&visible){Shape shape;shape.mesh=mesh;shape.local=local;result.push_back(std::move(shape));}
  });
  return shapes.emplace(data,std::move(result)).first->second;
 }
 UObject* GlowMaterial(){
  if(auto* m=material.Get())return m;
  auto* owner=coordinator.Get();if(!Live(owner))throw std::runtime_error("Building overlay provider expired");
  Call load(owner,L"RuneSchemaBuildingOverlayMaterial");load.Invoke();auto* value=load.Result();
  if(!value)throw std::runtime_error("Protection light material missing");material=value;return value;
 }
 UObject* Actor(UObject* c){
  if(auto* current=actor.Get())return current;
  auto* gameplay=Find<UObject>(L"/Script/Engine.Default__GameplayStatics");auto transform=Identity();Call spawn(gameplay,L"BeginDeferredActorSpawnFromClass");spawn.Object(L"WorldContextObject",c);spawn.Object(L"ActorClass",Find<UClass>(L"/Script/Engine.Actor"));spawn.Copy(L"SpawnTransform",&transform);spawn.Text(L"CollisionHandlingOverride",L"AlwaysSpawn");spawn.Invoke();
  auto* value=spawn.Result();if(!value)throw std::runtime_error("Protection light owner could not spawn");
  auto* tags=Field(value->GetClassPrivate(),L"Tags");if(!tags->ImportText_Direct(L"(RuneSchemaBuildingOverlay)",tags->ContainerPtrToValuePtr<void>(value),value,0,nullptr))throw std::runtime_error("Protection light tag could not be set");
  Call finish(gameplay,L"FinishSpawningActor");finish.Object(L"Actor",value);finish.Copy(L"SpawnTransform",&transform);finish.Invoke();actor=value;return value;
 }
 UObject* Component(UObject* c,UObject* mesh){
  auto& batch=batches[mesh];if(auto* existing=batch.component.Get())return existing;
  auto* owner=Actor(c);auto identity=Identity();Call create(owner,L"AddComponentByClass");create.Object(L"Class",Find<UClass>(L"/Script/Engine.InstancedStaticMeshComponent"));create.Text(L"bDeferredFinish",L"True");create.Copy(L"RelativeTransform",&identity);create.Invoke();auto* component=create.Result();
  if(!component)throw std::runtime_error("Protection light component could not spawn");
  Call collision(component,L"SetCollisionEnabled");collision.Text(L"NewType",L"NoCollision");collision.Invoke();
  Call overlap(component,L"SetGenerateOverlapEvents");overlap.Text(L"bInGenerateOverlapEvents",L"False");overlap.Invoke();
  Boolean(component->GetClassPrivate(),component,L"bCanEverAffectNavigation",false);Boolean(component->GetClassPrivate(),component,L"CastShadow",false);
  Call setMesh(component,L"SetStaticMesh");setMesh.Object(L"NewMesh",mesh);setMesh.Invoke();if(!setMesh.Bool())throw std::runtime_error("Protection light mesh assignment failed");
  Call count(component,L"GetNumMaterials");count.Invoke();int slots=*static_cast<int*>(count.Data(L"ReturnValue"));
  for(int i=0;i<std::max(1,slots);++i){Call tint(component,L"SetMaterial");tint.Text(L"ElementIndex",std::to_wstring(i).c_str());tint.Object(L"Material",GlowMaterial());tint.Invoke();}
  Call finish(owner,L"FinishAddComponent");finish.Object(L"Component",component);finish.Copy(L"RelativeTransform",&identity);finish.Invoke();batch.component=component;return component;
 }
 static Transform Compose(const Transform& local,const Transform& world){
  Call compose(Find<UObject>(L"/Script/Engine.Default__KismetMathLibrary"),L"ComposeTransforms");compose.Copy(L"A",&local);compose.Copy(L"B",&world);compose.Invoke();return *static_cast<Transform*>(compose.Data(L"ReturnValue"));
 }
 void Upload(UObject* c,const std::map<UObject*,std::vector<Transform>>& grouped){
  auto equal=[](const std::vector<Transform>& a,const std::vector<Transform>& b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(std::memcmp(a[i].rotation,b[i].rotation,32)||std::memcmp(&a[i].location,&b[i].location,sizeof(Vec3))||std::memcmp(&a[i].scale,&b[i].scale,sizeof(Vec3)))return false;return true;};
  for(auto& [mesh,batch]:batches)if(!grouped.contains(mesh)&&!batch.previous.empty())if(auto* component=batch.component.Get()){Call clear(component,L"ClearInstances");clear.Invoke();batch.previous.clear();}
  for(auto& [mesh,transforms]:grouped)if(!transforms.empty()){
   if(auto found=batches.find(mesh);found!=batches.end()&&equal(found->second.previous,transforms))continue;
   auto* component=Component(c,mesh);Call clear(component,L"ClearInstances");clear.Invoke();Call add(component,L"AddInstances");auto* p=CastField<FArrayProperty>(add.Prop(L"InstanceTransforms"));
   if(!p||p->GetInner()->GetElementSize()!=sizeof(Transform))throw std::runtime_error("Protection light instance transforms differ");
   auto* array=static_cast<FScriptArray*>(add.Data(L"InstanceTransforms"));auto* inner=p->GetInner();array->Add(static_cast<int>(transforms.size()),inner->GetElementSize(),inner->GetMinAlignment());
   for(size_t i=0;i<transforms.size();++i){auto* value=static_cast<char*>(array->GetData())+i*inner->GetElementSize();inner->InitializeValue(value);inner->CopyCompleteValue(value,&transforms[i]);}
   add.Text(L"bWorldSpace",L"True");add.Text(L"bUpdateNavigation",L"False");add.Invoke();
   batches[mesh].previous=transforms;
  }
 }
public:
 void Clear(){if(auto* owner=actor.Get();Live(owner)){Call destroy(owner,L"K2_DestroyActor");destroy.Invoke();}actor=nullptr;batches.clear();shapes.clear();}
 void Update(UObject* c,float delta){
  elapsed+=delta;if(elapsed<.5f)return;elapsed=0;
  Call dedicated(Find<UObject>(L"/Script/Engine.Default__KismetSystemLibrary"),L"IsDedicatedServer");dedicated.Object(L"WorldContextObject",c);dedicated.Invoke();if(dedicated.Bool())return;
  if(coordinator.Get()!=c){Clear();material=nullptr;coordinator=c;}
  Call pawn(Find<UObject>(L"/Script/Engine.Default__GameplayStatics"),L"GetPlayerPawn");pawn.Object(L"WorldContextObject",c);pawn.Text(L"PlayerIndex",L"0");pawn.Invoke();auto* player=pawn.Result();if(!Live(player))return;
  std::map<UObject*,std::vector<Transform>> grouped;
  std::vector<UObject*> managers;UObjectGlobals::FindAllOf(L"GlobalBuildingManager",managers);auto* type=Find<UScriptStruct>(L"/Script/Dominion.LightweightBuildingPiece");
  for(auto* manager:managers)if(Live(manager)&&manager->GetWorld()==c->GetWorld()){
   auto* lw=Object(manager,L"LightweightBuildingPieceManager");if(!Live(lw))continue;void* iface=nullptr;
   for(auto& i:lw->GetClassPrivate()->GetInterfaces())if(i.Class->GetName()==L"BuildingPieceResolver")iface=reinterpret_cast<char*>(lw)+i.PointerOffset;
   if(!iface)throw std::runtime_error("Lightweight render resolver missing");
   auto* code=static_cast<unsigned char*>((*static_cast<void***>(iface))[2]);const unsigned char signature[]={0x8b,0x41,0x28,0x4c,0x8b,0xc9,0x44,0x8b,0xd2,0x3b,0x41,0x54};
   if(std::memcmp(code,signature,sizeof(signature)))throw std::runtime_error("Lightweight render map signature differs");
   auto* map=reinterpret_cast<FScriptMap*>(static_cast<char*>(iface)+0x20);auto layout=FScriptMap::GetScriptLayout(4,4,8,8);if(map->GetMaxIndex()<0||map->GetMaxIndex()>2000000)throw std::runtime_error("Invalid lightweight render map count");
   auto lookup=reinterpret_cast<void*(*)(void*,unsigned)>(code);
   for(int i=0;i<map->GetMaxIndex();++i)if(map->IsValidIndex(i)){
    auto* pair=static_cast<char*>(map->GetData(i,layout));auto id=*reinterpret_cast<unsigned*>(pair);auto* piece=*reinterpret_cast<void**>(pair+layout.ValueOffset);
    if(lookup(iface,id)!=piece||!piece||*static_cast<unsigned*>(Value(type,piece,L"PieceID"))!=id)throw std::runtime_error("Lightweight render identity differs");
    auto world=*static_cast<Transform*>(Value(type,piece,L"Transform"));if(!ProviderBool(c,L"RuneSchemaBuildingOverlayEnabled",world.location)||Boolean(type,piece,L"bIsPreview")||Boolean(type,piece,L"bIsGhosted"))continue;
    for(auto& shape:Geometry(ObjectField(type,piece,L"BuildingPieceData")))if(auto* mesh=shape.mesh.Get())grouped[mesh].push_back(Compose(shape.local,world));
   }
  }
  std::vector<UObject*> buildings;UObjectGlobals::FindAllOf(L"BaseBuildingActor",buildings);
  for(auto* building:buildings)if(Live(building)&&building->GetWorld()==c->GetWorld()&&!Boolean(building->GetClassPrivate(),building,L"bIsPreview")&&!Boolean(building->GetClassPrivate(),building,L"bIsGhosted")){
   Call position(building,L"K2_GetActorLocation");position.Invoke();auto p=*static_cast<Vec3*>(position.Data(L"ReturnValue"));if(!ProviderBool(c,L"RuneSchemaBuildingOverlayEnabled",p))continue;
   Call components(building,L"K2_GetComponentsByClass");components.Object(L"ComponentClass",Find<UClass>(L"/Script/Engine.StaticMeshComponent"));components.Invoke();auto* array=static_cast<FScriptArray*>(components.Data(L"ReturnValue"));auto** list=static_cast<UObject**>(array->GetData());
   for(int i=0;i<array->Num();++i)if(Live(list[i])&&Boolean(list[i]->GetClassPrivate(),list[i],L"bVisible")&&!Boolean(list[i]->GetClassPrivate(),list[i],L"bHiddenInGame"))if(auto* mesh=Object(list[i],L"StaticMesh")){
    Call transform(list[i],L"K2_GetComponentToWorld");transform.Invoke();grouped[mesh].push_back(*static_cast<Transform*>(transform.Data(L"ReturnValue")));
   }
  }
  Upload(c,grouped);
 }
};
}
