#include "vfx_particles.h"
#include "vfx_math.h"

#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/core/memory.hpp>

using namespace godot;

// ============================================================================
// BINDING
// ============================================================================

void VFXParticles3D::_bind_methods() {
	// Core
	ClassDB::bind_method(D_METHOD("set_amount", "amount"), &VFXParticles3D::set_amount);
	ClassDB::bind_method(D_METHOD("get_amount"), &VFXParticles3D::get_amount);
	ClassDB::bind_method(D_METHOD("set_lifetime", "lifetime"), &VFXParticles3D::set_lifetime);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &VFXParticles3D::get_lifetime);
	ClassDB::bind_method(D_METHOD("set_one_shot", "one_shot"), &VFXParticles3D::set_one_shot);
	ClassDB::bind_method(D_METHOD("get_one_shot"), &VFXParticles3D::get_one_shot);
	ClassDB::bind_method(D_METHOD("set_emitting", "emitting"), &VFXParticles3D::set_emitting);
	ClassDB::bind_method(D_METHOD("get_emitting"), &VFXParticles3D::get_emitting);
	ClassDB::bind_method(D_METHOD("set_explosiveness_ratio", "ratio"), &VFXParticles3D::set_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("get_explosiveness_ratio"), &VFXParticles3D::get_explosiveness_ratio);
	ClassDB::bind_method(D_METHOD("set_randomness_ratio", "ratio"), &VFXParticles3D::set_randomness_ratio);
	ClassDB::bind_method(D_METHOD("get_randomness_ratio"), &VFXParticles3D::get_randomness_ratio);
	ClassDB::bind_method(D_METHOD("set_lifetime_randomness", "randomness"), &VFXParticles3D::set_lifetime_randomness);
	ClassDB::bind_method(D_METHOD("get_lifetime_randomness"), &VFXParticles3D::get_lifetime_randomness);

	// Emission
	ClassDB::bind_method(D_METHOD("set_emission_shape", "shape"), &VFXParticles3D::set_emission_shape);
	ClassDB::bind_method(D_METHOD("get_emission_shape"), &VFXParticles3D::get_emission_shape);
	ClassDB::bind_method(D_METHOD("set_emission_sphere_radius", "radius"), &VFXParticles3D::set_emission_sphere_radius);
	ClassDB::bind_method(D_METHOD("get_emission_sphere_radius"), &VFXParticles3D::get_emission_sphere_radius);
	ClassDB::bind_method(D_METHOD("set_emission_box_extents", "extents"), &VFXParticles3D::set_emission_box_extents);
	ClassDB::bind_method(D_METHOD("get_emission_box_extents"), &VFXParticles3D::get_emission_box_extents);
	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &VFXParticles3D::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &VFXParticles3D::get_direction);
	ClassDB::bind_method(D_METHOD("set_spread", "degrees"), &VFXParticles3D::set_spread);
	ClassDB::bind_method(D_METHOD("get_spread"), &VFXParticles3D::get_spread);
	ClassDB::bind_method(D_METHOD("set_flatness", "flatness"), &VFXParticles3D::set_flatness);
	ClassDB::bind_method(D_METHOD("get_flatness"), &VFXParticles3D::get_flatness);

	// Physics
	ClassDB::bind_method(D_METHOD("set_gravity", "gravity"), &VFXParticles3D::set_gravity);
	ClassDB::bind_method(D_METHOD("get_gravity"), &VFXParticles3D::get_gravity);
	ClassDB::bind_method(D_METHOD("set_initial_velocity_min", "velocity"), &VFXParticles3D::set_initial_velocity_min);
	ClassDB::bind_method(D_METHOD("get_initial_velocity_min"), &VFXParticles3D::get_initial_velocity_min);
	ClassDB::bind_method(D_METHOD("set_initial_velocity_max", "velocity"), &VFXParticles3D::set_initial_velocity_max);
	ClassDB::bind_method(D_METHOD("get_initial_velocity_max"), &VFXParticles3D::get_initial_velocity_max);
	ClassDB::bind_method(D_METHOD("set_damping_min", "damping"), &VFXParticles3D::set_damping_min);
	ClassDB::bind_method(D_METHOD("get_damping_min"), &VFXParticles3D::get_damping_min);
	ClassDB::bind_method(D_METHOD("set_damping_max", "damping"), &VFXParticles3D::set_damping_max);
	ClassDB::bind_method(D_METHOD("get_damping_max"), &VFXParticles3D::get_damping_max);

	// Appearance
	ClassDB::bind_method(D_METHOD("set_scale_amount_min", "scale"), &VFXParticles3D::set_scale_amount_min);
	ClassDB::bind_method(D_METHOD("get_scale_amount_min"), &VFXParticles3D::get_scale_amount_min);
	ClassDB::bind_method(D_METHOD("set_scale_amount_max", "scale"), &VFXParticles3D::set_scale_amount_max);
	ClassDB::bind_method(D_METHOD("get_scale_amount_max"), &VFXParticles3D::get_scale_amount_max);
	ClassDB::bind_method(D_METHOD("set_scale_curve", "curve"), &VFXParticles3D::set_scale_curve);
	ClassDB::bind_method(D_METHOD("get_scale_curve"), &VFXParticles3D::get_scale_curve);
	ClassDB::bind_method(D_METHOD("set_color_ramp", "gradient"), &VFXParticles3D::set_color_ramp);
	ClassDB::bind_method(D_METHOD("get_color_ramp"), &VFXParticles3D::get_color_ramp);

	// Collision
	ClassDB::bind_method(D_METHOD("set_collision_mode", "mode"), &VFXParticles3D::set_collision_mode);
	ClassDB::bind_method(D_METHOD("get_collision_mode"), &VFXParticles3D::get_collision_mode);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &VFXParticles3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &VFXParticles3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_bounce", "bounce"), &VFXParticles3D::set_collision_bounce);
	ClassDB::bind_method(D_METHOD("get_collision_bounce"), &VFXParticles3D::get_collision_bounce);
	ClassDB::bind_method(D_METHOD("set_collision_friction", "friction"), &VFXParticles3D::set_collision_friction);
	ClassDB::bind_method(D_METHOD("get_collision_friction"), &VFXParticles3D::get_collision_friction);
	ClassDB::bind_method(D_METHOD("set_collision_plane", "plane"), &VFXParticles3D::set_collision_plane);
	ClassDB::bind_method(D_METHOD("get_collision_plane"), &VFXParticles3D::get_collision_plane);
	ClassDB::bind_method(D_METHOD("set_collision_sphere_center", "center"), &VFXParticles3D::set_collision_sphere_center);
	ClassDB::bind_method(D_METHOD("get_collision_sphere_center"), &VFXParticles3D::get_collision_sphere_center);
	ClassDB::bind_method(D_METHOD("set_collision_sphere_radius", "radius"), &VFXParticles3D::set_collision_sphere_radius);
	ClassDB::bind_method(D_METHOD("get_collision_sphere_radius"), &VFXParticles3D::get_collision_sphere_radius);

	// Subemitters — raw pointer because VFXParticles3D is a Node, not RefCounted
	ClassDB::bind_method(D_METHOD("add_subemitter", "system", "trigger", "count", "probability"), &VFXParticles3D::add_subemitter);
	ClassDB::bind_method(D_METHOD("clear_subemitters"), &VFXParticles3D::clear_subemitters);
	ClassDB::bind_method(D_METHOD("get_subemitter_count"), &VFXParticles3D::get_subemitter_count);

	// Actions
	ClassDB::bind_method(D_METHOD("restart"), &VFXParticles3D::restart);
	ClassDB::bind_method(D_METHOD("emit_burst", "count"), &VFXParticles3D::emit_burst);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &VFXParticles3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &VFXParticles3D::get_material);

	// Properties
	ADD_GROUP("Emission", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "amount", PROPERTY_HINT_RANGE, "1,100000,1"), "set_amount", "get_amount");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime", PROPERTY_HINT_RANGE, "0.01,600.0,0.01,or_greater"), "set_lifetime", "get_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lifetime_randomness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_lifetime_randomness", "get_lifetime_randomness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "explosiveness_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_explosiveness_ratio", "get_explosiveness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "randomness_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_randomness_ratio", "get_randomness_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "emission_shape", PROPERTY_HINT_ENUM, "Point,Sphere,Box"), "set_emission_shape", "get_emission_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_sphere_radius", PROPERTY_HINT_RANGE, "0.01,128.0,0.01,or_greater"), "set_emission_sphere_radius", "get_emission_sphere_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_box_extents"), "set_emission_box_extents", "get_emission_box_extents");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spread", PROPERTY_HINT_RANGE, "0,180.0,0.1"), "set_spread", "get_spread");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flatness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_flatness", "get_flatness");

	ADD_GROUP("Particle Flags", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "one_shot"), "set_one_shot", "get_one_shot");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting"), "set_emitting", "get_emitting");

	ADD_GROUP("Physics", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity"), "set_gravity", "get_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "initial_velocity_min", PROPERTY_HINT_RANGE, "-1000.0,1000.0,0.01,or_lesser,or_greater"), "set_initial_velocity_min", "get_initial_velocity_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "initial_velocity_max", PROPERTY_HINT_RANGE, "-1000.0,1000.0,0.01,or_lesser,or_greater"), "set_initial_velocity_max", "get_initial_velocity_max");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping_min", PROPERTY_HINT_RANGE, "0,100.0,0.01,or_greater"), "set_damping_min", "get_damping_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping_max", PROPERTY_HINT_RANGE, "0,100.0,0.01,or_greater"), "set_damping_max", "get_damping_max");

	ADD_GROUP("Appearance", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_amount_min", PROPERTY_HINT_RANGE, "0,1000.0,0.01,or_greater"), "set_scale_amount_min", "get_scale_amount_min");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "scale_amount_max", PROPERTY_HINT_RANGE, "0,1000.0,0.01,or_greater"), "set_scale_amount_max", "get_scale_amount_max");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scale_curve", PROPERTY_HINT_RESOURCE_TYPE, "Curve"), "set_scale_curve", "get_scale_curve");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "color_ramp", PROPERTY_HINT_RESOURCE_TYPE, "Gradient"), "set_color_ramp", "get_color_ramp");

	ADD_GROUP("Collision", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mode", PROPERTY_HINT_ENUM, "Disabled,Raycast,Plane,Sphere"), "set_collision_mode", "get_collision_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_bounce", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_collision_bounce", "get_collision_bounce");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_friction", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_collision_friction", "get_collision_friction");
	ADD_PROPERTY(PropertyInfo(Variant::PLANE, "collision_plane"), "set_collision_plane", "get_collision_plane");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "collision_sphere_center"), "set_collision_sphere_center", "get_collision_sphere_center");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_sphere_radius", PROPERTY_HINT_RANGE, "0.01,128.0,0.01,or_greater"), "set_collision_sphere_radius", "get_collision_sphere_radius");

	// Constants
	ClassDB::bind_integer_constant(get_class_static(), "", "EMISSION_POINT", EMISSION_POINT);
	ClassDB::bind_integer_constant(get_class_static(), "", "EMISSION_SPHERE", EMISSION_SPHERE);
	ClassDB::bind_integer_constant(get_class_static(), "", "EMISSION_BOX", EMISSION_BOX);

	ClassDB::bind_integer_constant(get_class_static(), "", "COLLISION_NONE", COLLISION_NONE);
	ClassDB::bind_integer_constant(get_class_static(), "", "COLLISION_RAYCAST", COLLISION_RAYCAST);
	ClassDB::bind_integer_constant(get_class_static(), "", "COLLISION_PLANE", COLLISION_PLANE);
	ClassDB::bind_integer_constant(get_class_static(), "", "COLLISION_SPHERE", COLLISION_SPHERE);

	ClassDB::bind_integer_constant(get_class_static(), "", "SUBEMIT_ON_BIRTH", SUBEMIT_ON_BIRTH);
	ClassDB::bind_integer_constant(get_class_static(), "", "SUBEMIT_ON_DEATH", SUBEMIT_ON_DEATH);
	ClassDB::bind_integer_constant(get_class_static(), "", "SUBEMIT_ON_COLLISION", SUBEMIT_ON_COLLISION);
}

// ============================================================================
// LIFECYCLE
// ============================================================================

VFXParticles3D::VFXParticles3D() {}

VFXParticles3D::~VFXParticles3D() {
	if (mesh_instance) {
		mesh_instance->queue_free();
	}
}

void VFXParticles3D::_notification(int p_what) {
	if (p_what == NOTIFICATION_READY) {
		set_process(true);

		mesh_instance = memnew(MeshInstance3D);
		add_child(mesh_instance);

		array_mesh.instantiate();
		mesh_instance->set_mesh(array_mesh);

		particles.resize(max_particles);
		r_verts.resize(max_particles * 4);
		r_normals.resize(max_particles * 4);
		r_colors.resize(max_particles * 4);
		r_uvs.resize(max_particles * 4);
		r_indices.resize(max_particles * 6);
	}
	if (p_what == NOTIFICATION_PROCESS) {
		float delta = get_process_delta_time();
		_simulate(delta);
		_rebuild_mesh();
	}
}

// ============================================================================
// SIMULATION
// ============================================================================

void VFXParticles3D::_simulate(float delta) {
	// Emission
	if (emitting) {
		if (one_shot) {
			if (!_one_shot_emitted) {
				_one_shot_emitted = true;
				emit_burst(max_particles);
			}
		} else {
			float rate_jitter = 1.0f + (Math::randf() - 0.5f) * 2.0f * randomness_ratio;
			emission_accum += emission_rate * delta * rate_jitter;
			int to_emit = (int)emission_accum;
			if (to_emit > 0) {
				emission_accum -= (float)to_emit;
				for (int i = 0; i < to_emit; i++) {
					_emit(1, _random_emission_position(), Vector3());
				}
			}
		}
	}

	// Update particles
	for (int i = 0; i < particle_count; i++) {
		VFXParticle& p = particles[i];
		p.age += delta;

		if (p.age >= p.lifetime) {
			_trigger_subemit(SUBEMIT_ON_DEATH, p.position, p.velocity.normalized());
			_kill(i);
			i--; // process swapped-in particle
			continue;
		}

		// Damping (seed-derived deterministic per-particle)
		float damping_rnd = (float)(p.seed % 10000) / 10000.0f;
		float damping = vfx::lerp(damping_min, damping_max, damping_rnd);
		if (damping > 0.0f) {
			p.velocity *= (1.0f - damping * delta);
		}

		// Gravity + acceleration
		p.velocity += (gravity + p.acceleration) * delta;

		// Integrate
		Vector3 next_pos = p.position + p.velocity * delta;

		// Collision
		if (collision_mode != COLLISION_NONE) {
			if (_solve_collision(p, delta)) {
				_trigger_subemit(SUBEMIT_ON_COLLISION, p.position, -p.velocity.normalized());
			}
		} else {
			p.position = next_pos;
		}
	}

	// One-shot auto-stop
	if (one_shot && _one_shot_emitted && particle_count == 0) {
		emitting = false;
	}
}

void VFXParticles3D::_emit(int count, const Vector3& pos, const Vector3& normal) {
	for (int i = 0; i < count; i++) {
		if (particle_count >= max_particles) break;

		VFXParticle& p = particles[particle_count];
		p.active = true;
		p.age = 0.0f;
		p.lifetime = lifetime * (1.0f + (Math::randf() - 0.5f) * 2.0f * lifetime_randomness);
		p.lifetime = vfx::clampf(p.lifetime, 0.001f, 600.0f);
		p.position = pos;
		p.seed = (uint32_t)(Math::randf() * 4294967295.0f);

		// Velocity
		Vector3 dir = _random_direction_in_cone();
		if (flatness > 0.0f) {
			Vector3 proj = vfx::project_on_plane(dir, emission_direction.normalized());
			dir = dir.lerp(proj, flatness).normalized();
		}
		float speed = vfx::lerp(initial_velocity_min, initial_velocity_max, Math::randf());
		p.velocity = dir * speed;

		p.acceleration = Vector3(0, 0, 0);
		p.color = color_ramp.is_valid() ? color_ramp->get_color(0.0f) : Color(1, 1, 1, 1);
		p.size = vfx::lerp(scale_amount_min, scale_amount_max, Math::randf());
		p.rotation = 0.0f;

		particle_count++;
	}

	if (count > 0) {
		_trigger_subemit(SUBEMIT_ON_BIRTH, pos, normal);
	}
}

void VFXParticles3D::_kill(int idx) {
	if (idx < 0 || idx >= particle_count) return;
	particles[idx] = particles[particle_count - 1];
	particle_count--;
}

// ============================================================================
// RENDERING
// ============================================================================

void VFXParticles3D::_rebuild_mesh() {
	if (particle_count == 0) {
		if (array_mesh.is_valid() && array_mesh->get_surface_count() > 0) {
			array_mesh->clear_surfaces();
		}
		return;
	}

	// Camera for billboarding
	Camera3D* cam = nullptr;
	if (get_viewport()) {
		cam = get_viewport()->get_camera_3d();
	}

	Vector3 cam_pos_local = Vector3(0, 0, 5); // fallback
	if (cam) {
		cam_pos_local = to_local(cam->get_global_position());
	}

	r_verts.resize(particle_count * 4);
	r_normals.resize(particle_count * 4);
	r_colors.resize(particle_count * 4);
	r_uvs.resize(particle_count * 4);
	r_indices.resize(particle_count * 6);

	for (int i = 0; i < particle_count; i++) {
		const VFXParticle& p = particles[i];

		// Evaluate curves
		float life_t = (p.lifetime > 0.0f) ? vfx::clampf(p.age / p.lifetime, 0.0f, 1.0f) : 0.0f;
		Color col = p.color;
		if (color_ramp.is_valid()) {
			col = color_ramp->get_color(life_t);
		}
		float sz = p.size;
		if (scale_curve.is_valid()) {
			sz *= scale_curve->sample_baked(life_t);
		}

		// Billboard basis
		Vector3 to_cam = (cam_pos_local - p.position).normalized();
		if (to_cam.length_squared() < 0.0001f) to_cam = Vector3(0, 0, 1);
		Basis bb = vfx::look_at_safe(to_cam);
		Vector3 right = bb.get_column(0) * sz * 0.5f;
		Vector3 up = bb.get_column(1) * sz * 0.5f;

		// Apply particle rotation around view axis
		if (p.rotation != 0.0f) {
			Basis rot_basis(to_cam, p.rotation);
			right = rot_basis.xform(right);
			up = rot_basis.xform(up);
		}

		int v = i * 4;
		r_verts[v + 0] = p.position - right - up;
		r_verts[v + 1] = p.position + right - up;
		r_verts[v + 2] = p.position + right + up;
		r_verts[v + 3] = p.position - right + up;

		Vector3 normal = -to_cam;
		for (int j = 0; j < 4; j++) {
			r_normals[v + j] = normal;
			r_colors[v + j] = col;
		}

		r_uvs[v + 0] = Vector2(0, 0);
		r_uvs[v + 1] = Vector2(1, 0);
		r_uvs[v + 2] = Vector2(1, 1);
		r_uvs[v + 3] = Vector2(0, 1);

		int idx = i * 6;
		r_indices[idx + 0] = v + 0;
		r_indices[idx + 1] = v + 1;
		r_indices[idx + 2] = v + 2;
		r_indices[idx + 3] = v + 0;
		r_indices[idx + 4] = v + 2;
		r_indices[idx + 5] = v + 3;
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = r_verts;
	arrays[Mesh::ARRAY_NORMAL] = r_normals;
	arrays[Mesh::ARRAY_COLOR] = r_colors;
	arrays[Mesh::ARRAY_TEX_UV] = r_uvs;
	arrays[Mesh::ARRAY_INDEX] = r_indices;

	array_mesh->clear_surfaces();
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
}

// ============================================================================
// COLLISION
// ============================================================================

bool VFXParticles3D::_solve_collision(VFXParticle& p, float delta) {
	Vector3 next_pos = p.position + p.velocity * delta;

	switch (collision_mode) {
		case COLLISION_RAYCAST: {
			Ref<World3D> world = get_world_3d();
			if (world.is_null()) return false;
			PhysicsDirectSpaceState3D* space = world->get_direct_space_state();
			if (!space) return false;

			Transform3D world_xform = get_global_transform();
			Vector3 w_from = world_xform.xform(p.position);
			Vector3 w_to = world_xform.xform(next_pos);

			Ref<PhysicsRayQueryParameters3D> params;
			params.instantiate();
			params->set_from(w_from);
			params->set_to(w_to);
			params->set_collision_mask(collision_mask);
			params->set_collide_with_bodies(true);
			params->set_collide_with_areas(false);

			Dictionary result = space->intersect_ray(params);
			if (result.is_empty()) {
				p.position = next_pos;
				return false;
			}

			Vector3 hit_pos = result["position"];
			Vector3 hit_normal = result["normal"];
			p.position = to_local(hit_pos + hit_normal * 0.001f);

			Vector3 v = p.velocity;
			Vector3 vn = hit_normal * v.dot(hit_normal);
			Vector3 vt = v - vn;
			p.velocity = vt * (1.0f - collision_friction) - vn * collision_bounce;
			return true;
		}

		case COLLISION_PLANE: {
			float d_before = collision_plane.distance_to(p.position);
			float d_after = collision_plane.distance_to(next_pos);
			if ((d_before >= 0.0f && d_after >= 0.0f) || (d_before < 0.0f && d_after < 0.0f)) {
				p.position = next_pos;
				return false;
			}
			// Crossed plane
			Vector3 hit = collision_plane.project(p.position);
			p.position = hit + collision_plane.get_normal() * 0.001f;

			Vector3 n = collision_plane.get_normal();
			Vector3 v = p.velocity;
			Vector3 vn = n * v.dot(n);
			Vector3 vt = v - vn;
			p.velocity = vt * (1.0f - collision_friction) - vn * collision_bounce;
			return true;
		}

		case COLLISION_SPHERE: {
			float d_before = (p.position - collision_sphere_center).length() - collision_sphere_radius;
			float d_after = (next_pos - collision_sphere_center).length() - collision_sphere_radius;
			if (d_after >= 0.0f) {
				p.position = next_pos;
				return false;
			}
			// Inside or crossed into sphere
			Vector3 n = (p.position - collision_sphere_center).normalized();
			if (n.length_squared() < 0.0001f) n = Vector3(0, 1, 0);
			p.position = collision_sphere_center + n * (collision_sphere_radius + 0.001f);

			Vector3 v = p.velocity;
			Vector3 vn = n * v.dot(n);
			Vector3 vt = v - vn;
			p.velocity = vt * (1.0f - collision_friction) - vn * collision_bounce;
			return true;
		}

		default:
			p.position = next_pos;
			return false;
	}
}

// ============================================================================
// SUBEMITTERS
// ============================================================================

void VFXParticles3D::_trigger_subemit(SubEmitTrigger trigger, const Vector3& pos, const Vector3& normal) {
	for (const auto& sub : sub_emitters) {
		if (sub.trigger != trigger) continue;
		if (Math::randf() > sub.probability) continue;
		if (!sub.system) continue;

		// Validate pointer with cast (returns null if object was freed)
		VFXParticles3D* valid = Object::cast_to<VFXParticles3D>(sub.system);
		if (!valid) continue;

		// Position sub-emitter in world space at event location
		Vector3 world_pos = get_global_transform().xform(pos);
		valid->set_global_position(world_pos);
		valid->emit_burst(sub.count);
	}
}

// ============================================================================
// RANDOM HELPERS
// ============================================================================

Vector3 VFXParticles3D::_random_direction_in_cone() const {
	Vector3 dir = emission_direction.normalized();
	if (spread_degrees <= 0.0f) return dir;

	Basis basis = vfx::look_at_safe(dir);
	float spread_rad = Math::deg_to_rad(spread_degrees);

	float cos_theta = 1.0f - Math::randf() * (1.0f - Math::cos(spread_rad));
	float theta = Math::acos(cos_theta);
	float phi = Math::randf() * 2.0f * Math::PI;

	Vector3 local_dir = Vector3(
		Math::sin(theta) * Math::cos(phi),
		Math::sin(theta) * Math::sin(phi),
		Math::cos(theta)
	);
	return basis.xform(local_dir);
}

Vector3 VFXParticles3D::_random_emission_position() const {
	switch (emission_shape) {
		case EMISSION_SPHERE: {
			Vector3 rnd = Vector3(Math::randf() - 0.5f, Math::randf() - 0.5f, Math::randf() - 0.5f).normalized();
			return rnd * Math::randf() * emission_sphere_radius;
		}
		case EMISSION_BOX: {
			return Vector3(
				(Math::randf() - 0.5f) * 2.0f * emission_box_extents.x,
				(Math::randf() - 0.5f) * 2.0f * emission_box_extents.y,
				(Math::randf() - 0.5f) * 2.0f * emission_box_extents.z
			);
		}
		case EMISSION_POINT:
		default:
			return Vector3(0, 0, 0);
	}
}

// ============================================================================
// GETTERS / SETTERS
// ============================================================================

void VFXParticles3D::set_amount(int p_amount) {
	if (p_amount < 1) p_amount = 1;
	max_particles = p_amount;
	particles.resize(max_particles);
	if (particle_count > max_particles) particle_count = max_particles;
	r_verts.resize(max_particles * 4);
	r_normals.resize(max_particles * 4);
	r_colors.resize(max_particles * 4);
	r_uvs.resize(max_particles * 4);
	r_indices.resize(max_particles * 6);
}
int VFXParticles3D::get_amount() const { return max_particles; }

void VFXParticles3D::set_lifetime(float p_lifetime) { lifetime = vfx::clampf(p_lifetime, 0.001f, 600.0f); }
float VFXParticles3D::get_lifetime() const { return lifetime; }

void VFXParticles3D::set_one_shot(bool p_one_shot) { one_shot = p_one_shot; }
bool VFXParticles3D::get_one_shot() const { return one_shot; }

void VFXParticles3D::set_emitting(bool p_emitting) { emitting = p_emitting; }
bool VFXParticles3D::get_emitting() const { return emitting; }

void VFXParticles3D::set_explosiveness_ratio(float ratio) { explosiveness_ratio = vfx::clampf(ratio, 0.0f, 1.0f); }
float VFXParticles3D::get_explosiveness_ratio() const { return explosiveness_ratio; }

void VFXParticles3D::set_randomness_ratio(float ratio) { randomness_ratio = vfx::clampf(ratio, 0.0f, 1.0f); }
float VFXParticles3D::get_randomness_ratio() const { return randomness_ratio; }

void VFXParticles3D::set_lifetime_randomness(float val) { lifetime_randomness = vfx::clampf(val, 0.0f, 1.0f); }
float VFXParticles3D::get_lifetime_randomness() const { return lifetime_randomness; }

void VFXParticles3D::set_emission_shape(int shape) { emission_shape = (EmissionShape)vfx::clampf(shape, 0, 2); }
int VFXParticles3D::get_emission_shape() const { return (int)emission_shape; }

void VFXParticles3D::set_emission_sphere_radius(float radius) { emission_sphere_radius = vfx::clampf(radius, 0.0f, 1000.0f); }
float VFXParticles3D::get_emission_sphere_radius() const { return emission_sphere_radius; }

void VFXParticles3D::set_emission_box_extents(const Vector3& extents) { emission_box_extents = extents; }
Vector3 VFXParticles3D::get_emission_box_extents() const { return emission_box_extents; }

void VFXParticles3D::set_direction(const Vector3& dir) { emission_direction = dir; }
Vector3 VFXParticles3D::get_direction() const { return emission_direction; }

void VFXParticles3D::set_spread(float degrees) { spread_degrees = vfx::clampf(degrees, 0.0f, 180.0f); }
float VFXParticles3D::get_spread() const { return spread_degrees; }

void VFXParticles3D::set_flatness(float f) { flatness = vfx::clampf(f, 0.0f, 1.0f); }
float VFXParticles3D::get_flatness() const { return flatness; }

void VFXParticles3D::set_gravity(const Vector3& g) { gravity = g; }
Vector3 VFXParticles3D::get_gravity() const { return gravity; }

void VFXParticles3D::set_initial_velocity_min(float v) { initial_velocity_min = v; }
float VFXParticles3D::get_initial_velocity_min() const { return initial_velocity_min; }

void VFXParticles3D::set_initial_velocity_max(float v) { initial_velocity_max = v; }
float VFXParticles3D::get_initial_velocity_max() const { return initial_velocity_max; }

void VFXParticles3D::set_damping_min(float v) { damping_min = vfx::clampf(v, 0.0f, 1000.0f); }
float VFXParticles3D::get_damping_min() const { return damping_min; }

void VFXParticles3D::set_damping_max(float v) { damping_max = vfx::clampf(v, 0.0f, 1000.0f); }
float VFXParticles3D::get_damping_max() const { return damping_max; }

void VFXParticles3D::set_scale_amount_min(float v) { scale_amount_min = vfx::clampf(v, 0.0f, 1000.0f); }
float VFXParticles3D::get_scale_amount_min() const { return scale_amount_min; }

void VFXParticles3D::set_scale_amount_max(float v) { scale_amount_max = vfx::clampf(v, 0.0f, 1000.0f); }
float VFXParticles3D::get_scale_amount_max() const { return scale_amount_max; }

void VFXParticles3D::set_scale_curve(const Ref<Curve>& curve) { scale_curve = curve; }
Ref<Curve> VFXParticles3D::get_scale_curve() const { return scale_curve; }

void VFXParticles3D::set_color_ramp(const Ref<Gradient>& ramp) { color_ramp = ramp; }
Ref<Gradient> VFXParticles3D::get_color_ramp() const { return color_ramp; }

void VFXParticles3D::set_collision_mode(int mode) { collision_mode = (CollisionMode)vfx::clampf(mode, 0, 3); }
int VFXParticles3D::get_collision_mode() const { return (int)collision_mode; }

void VFXParticles3D::set_collision_mask(int mask) { collision_mask = mask; }
int VFXParticles3D::get_collision_mask() const { return collision_mask; }

void VFXParticles3D::set_collision_bounce(float b) { collision_bounce = vfx::clampf(b, 0.0f, 1.0f); }
float VFXParticles3D::get_collision_bounce() const { return collision_bounce; }

void VFXParticles3D::set_collision_friction(float f) { collision_friction = vfx::clampf(f, 0.0f, 1.0f); }
float VFXParticles3D::get_collision_friction() const { return collision_friction; }

void VFXParticles3D::set_collision_plane(const Plane& p) { collision_plane = p; }
Plane VFXParticles3D::get_collision_plane() const { return collision_plane; }

void VFXParticles3D::set_collision_sphere_center(const Vector3& c) { collision_sphere_center = c; }
Vector3 VFXParticles3D::get_collision_sphere_center() const { return collision_sphere_center; }

void VFXParticles3D::set_collision_sphere_radius(float r) { collision_sphere_radius = vfx::clampf(r, 0.0f, 1000.0f); }
float VFXParticles3D::get_collision_sphere_radius() const { return collision_sphere_radius; }

void VFXParticles3D::add_subemitter(VFXParticles3D* system, int trigger, int count, float probability) {
	if (!system) return;
	SubEmitter se;
	se.system = system;
	se.trigger = (SubEmitTrigger)vfx::clampf(trigger, 0, 2);
	se.count = count > 0 ? count : 1;
	se.probability = vfx::clampf(probability, 0.0f, 1.0f);
	sub_emitters.push_back(se);
}

void VFXParticles3D::clear_subemitters() { sub_emitters.clear(); }
int VFXParticles3D::get_subemitter_count() const { return (int)sub_emitters.size(); }

void VFXParticles3D::restart() {
	particle_count = 0;
	emission_accum = 0.0f;
	_one_shot_emitted = false;
	emitting = true;
	if (array_mesh.is_valid() && array_mesh->get_surface_count() > 0) {
		array_mesh->clear_surfaces();
	}
}

void VFXParticles3D::emit_burst(int count) {
	for (int i = 0; i < count; i++) {
		_emit(1, _random_emission_position(), Vector3());
	}
}

void VFXParticles3D::set_material(const Ref<Material>& mat) {
	if (mesh_instance) {
		mesh_instance->set_material_override(mat);
	}
}

Ref<Material> VFXParticles3D::get_material() const {
	if (mesh_instance) {
		return mesh_instance->get_material_override();
	}
	return Ref<Material>();
}
