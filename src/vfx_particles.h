#ifndef VFX_PARTICLES_H
#define VFX_PARTICLES_H

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <vector>

using namespace godot;

struct VFXParticle {
	float lifetime = 1.0f;
	float age = 0.0f;
	Vector3 position;
	Vector3 velocity;
	Vector3 acceleration;
	Color color = Color(1, 1, 1, 1);
	float size = 1.0f;
	float rotation = 0.0f;
	bool active = false;
	uint32_t seed = 0;
};

class VFXParticles3D : public Node3D {
	GDCLASS(VFXParticles3D, Node3D)

public:
	enum EmissionShape {
		EMISSION_POINT = 0,
		EMISSION_SPHERE = 1,
		EMISSION_BOX = 2
	};

	enum CollisionMode {
		COLLISION_NONE = 0,
		COLLISION_RAYCAST = 1,
		COLLISION_PLANE = 2,
		COLLISION_SPHERE = 3
	};

	enum SubEmitTrigger {
		SUBEMIT_ON_BIRTH = 0,
		SUBEMIT_ON_DEATH = 1,
		SUBEMIT_ON_COLLISION = 2
	};

private:
	// Simulation pool (active particles are contiguous at front)
	std::vector<VFXParticle> particles;
	int particle_count = 0;
	int max_particles = 1024;
	float emission_rate = 100.0f;
	float emission_accum = 0.0f;

	// Emission settings
	EmissionShape emission_shape = EMISSION_POINT;
	float emission_sphere_radius = 1.0f;
	Vector3 emission_box_extents = Vector3(1, 1, 1);
	Vector3 emission_direction = Vector3(0, 1, 0);
	float spread_degrees = 0.0f;
	float flatness = 0.0f;

	// Physics
	Vector3 gravity = Vector3(0, -9.8f, 0);
	float initial_velocity_min = 0.0f;
	float initial_velocity_max = 1.0f;
	float damping_min = 0.0f;
	float damping_max = 0.0f;
	float lifetime = 1.0f;
	float lifetime_randomness = 0.0f;
	float explosiveness_ratio = 0.0f;
	float randomness_ratio = 0.0f;
	bool one_shot = false;
	bool emitting = true;

	// Collision
	CollisionMode collision_mode = COLLISION_NONE;
	int collision_mask = 1;
	float collision_bounce = 0.3f;
	float collision_friction = 0.5f;
	Plane collision_plane = Plane(Vector3(0, 1, 0), 0.0f);
	Vector3 collision_sphere_center = Vector3(0, 0, 0);
	float collision_sphere_radius = 1.0f;

	// Appearance
	Ref<Curve> scale_curve;
	Ref<Gradient> color_ramp;
	float scale_amount_min = 1.0f;
	float scale_amount_max = 1.0f;

	// Rendering
	MeshInstance3D* mesh_instance = nullptr;
	Ref<ArrayMesh> array_mesh;

	// Pre-allocated render buffers
	PackedVector3Array r_verts;
	PackedVector3Array r_normals;
	PackedColorArray   r_colors;
	PackedVector2Array r_uvs;
	PackedInt32Array   r_indices;

	// Subemitters
	struct SubEmitter {
		Ref<VFXParticles3D> system;
		SubEmitTrigger trigger;
		int count = 1;
		float probability = 1.0f;
	};
	std::vector<SubEmitter> sub_emitters;

	// Internal state
	bool _one_shot_emitted = false;

	void _simulate(float delta);
	void _emit(int count, const Vector3& pos, const Vector3& normal);
	void _kill(int idx);
	void _rebuild_mesh();
	void _trigger_subemit(SubEmitTrigger trigger, const Vector3& pos, const Vector3& normal);
	bool _solve_collision(VFXParticle& p, float delta);
	Vector3 _random_direction_in_cone() const;
	Vector3 _random_emission_position() const;

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	VFXParticles3D();
	~VFXParticles3D();

	// Core control
	void set_amount(int p_amount);
	int get_amount() const;
	void set_lifetime(float p_lifetime);
	float get_lifetime() const;
	void set_one_shot(bool p_one_shot);
	bool get_one_shot() const;
	void set_emitting(bool p_emitting);
	bool get_emitting() const;
	void set_explosiveness_ratio(float ratio);
	float get_explosiveness_ratio() const;
	void set_randomness_ratio(float ratio);
	float get_randomness_ratio() const;
	void set_lifetime_randomness(float val);
	float get_lifetime_randomness() const;

	// Emission
	void set_emission_shape(int shape);
	int get_emission_shape() const;
	void set_emission_sphere_radius(float radius);
	float get_emission_sphere_radius() const;
	void set_emission_box_extents(const Vector3& extents);
	Vector3 get_emission_box_extents() const;
	void set_direction(const Vector3& dir);
	Vector3 get_direction() const;
	void set_spread(float degrees);
	float get_spread() const;
	void set_flatness(float flatness);
	float get_flatness() const;

	// Physics
	void set_gravity(const Vector3& g);
	Vector3 get_gravity() const;
	void set_initial_velocity_min(float v);
	float get_initial_velocity_min() const;
	void set_initial_velocity_max(float v);
	float get_initial_velocity_max() const;
	void set_damping_min(float v);
	float get_damping_min() const;
	void set_damping_max(float v);
	float get_damping_max() const;

	// Appearance
	void set_scale_amount_min(float v);
	float get_scale_amount_min() const;
	void set_scale_amount_max(float v);
	float get_scale_amount_max() const;
	void set_scale_curve(const Ref<Curve>& curve);
	Ref<Curve> get_scale_curve() const;
	void set_color_ramp(const Ref<Gradient>& ramp);
	Ref<Gradient> get_color_ramp() const;

	// Collision
	void set_collision_mode(int mode);
	int get_collision_mode() const;
	void set_collision_mask(int mask);
	int get_collision_mask() const;
	void set_collision_bounce(float b);
	float get_collision_bounce() const;
	void set_collision_friction(float f);
	float get_collision_friction() const;
	void set_collision_plane(const Plane& p);
	Plane get_collision_plane() const;
	void set_collision_sphere_center(const Vector3& c);
	Vector3 get_collision_sphere_center() const;
	void set_collision_sphere_radius(float r);
	float get_collision_sphere_radius() const;

	// Subemitters
	void add_subemitter(const Ref<VFXParticles3D>& system, int trigger, int count, float probability);
	void clear_subemitters();
	int get_subemitter_count() const;

	// Actions
	void restart();
	void emit_burst(int count);
	void set_material(const Ref<Material>& mat);
	Ref<Material> get_material() const;
};

VARIANT_ENUM_CAST(VFXParticles3D::EmissionShape);
VARIANT_ENUM_CAST(VFXParticles3D::CollisionMode);
VARIANT_ENUM_CAST(VFXParticles3D::SubEmitTrigger);

#endif
