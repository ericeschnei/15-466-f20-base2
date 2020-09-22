#include "PlayMode.hpp"

#include "CustomTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "glm/ext/matrix_transform.hpp"

#include <ctime>
#include <glm/gtc/type_ptr.hpp>

#include <random>
#include <cstdio>

GLuint pillar_meshes_for_custom_texture_program = 0;
Load< MeshBuffer > pillar_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("pillar.pnct"));
	pillar_meshes_for_custom_texture_program = ret->make_vao_for_program(custom_texture_program->program);
	return ret;
});

Load< Scene > pillar_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("pillar.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = pillar_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = custom_texture_program_pipeline;

		drawable.pipeline.vao = pillar_meshes_for_custom_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*pillar_scene) {
	//get pointers to leg for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();

	camera->transform->rotation = glm::quat(glm::vec3(x_default, 0.0f, z_default));
	camera->transform->position = glm::vec3(60.0f, 0.0f, 20.0f);
	size_t pillar_arr_ptr = 0;
	for (auto drawable = scene.drawables.begin(); drawable != scene.drawables.end(); drawable++) {
		Scene::Transform *transform = drawable->transform;
		if (transform->name.substr(0,6) == "Pillar") {
			pillars[pillar_arr_ptr] = transform;
			drawable->pipeline.set_uniforms = [pillar_arr_ptr, this](){
				glUniform1i(custom_texture_program->PILLAR_LERP_bool, 1);
				glUniform1f(custom_texture_program->PILLAR_LERP_AMT_float, this->color_lerp_for_shader_uniform[pillar_arr_ptr]);
				glUniformMatrix4x3fv(custom_texture_program->PILLAR_COLORS_mat4x3, 1, GL_FALSE, glm::value_ptr(this->PILLAR_COLORS));
				GL_ERRORS();
			};
			pillar_arr_ptr++;
		}

		if (transform->name == "Cube") {
			cube = transform;
			drawable->pipeline.set_uniforms = [](){
				glUniform1i(custom_texture_program->PILLAR_LERP_bool, 0);
			};
		}

	}
	if (pillar_arr_ptr != pillars.size()) {
		throw std::runtime_error("Expected " + std::to_string(pillars.size()) + " pillars, but only found " + std::to_string(pillar_arr_ptr));
	}

	for (size_t i = 0; i < pillars.size(); i++) {
		Scene::Transform *pillar = pillars[i];
		int x = (int)(i % pillar_width) - (int)(pillar_width / 2);
		int y = (int)(i / pillar_width) - (int)(pillar_width / 2);

		pillar->position = glm::vec3(x * pillar_distance, y * pillar_distance, 0);
		pillar_animation_time[i] = 0.0f;
	}


	player_tile_new = glm::ivec2((pillar_width / 2), (pillar_width / 2));
	player_tile_old = glm::ivec2((pillar_width / 2), (pillar_width / 2));
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_ESCAPE) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			return true;
		} else if (evt.key.keysym.sym == SDLK_r) {
			player_tile_old = glm::ivec2(4, 4);
			player_tile_new = player_tile_old;
			for (float &f : pillar_animation_time) {
				f = 0.0f;
			}
			time_survived = 0.0f;
			timer = 0.0f;
			cube_animation_time = 0.0f;
			alive = true;
			return true;
		// don't allow a move if we're already animating a move
		} else if (cube_animation_time == 0.0f) {
			if (evt.key.keysym.sym == SDLK_a) {
				left.downs += 1;
				left.pressed = true;
				if (player_tile_old.y > 0) {
					player_tile_new = player_tile_old + glm::ivec2(0, -1);
				}
			} else if (evt.key.keysym.sym == SDLK_d) {
				right.downs += 1;
				right.pressed = true;
				if (player_tile_old.y < (int)pillar_width - 1) {
					player_tile_new = player_tile_old + glm::ivec2(0, 1);
				}
			} else if (evt.key.keysym.sym == SDLK_w) {
				up.downs += 1;
				up.pressed = true;
				if (player_tile_old.x > 0) {
					player_tile_new = player_tile_old + glm::ivec2(-1, 0);
				}
			} else if (evt.key.keysym.sym == SDLK_s) {
				down.downs += 1;
				down.pressed = true;
				if (player_tile_old.x < (int)pillar_width - 1) {
					player_tile_new = player_tile_old + glm::ivec2(1, 0);
				}
			} else {
				return false;
			}

			cube_animation_time = 0.0001f;
			return true;
		}
	} else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_a) {
			left.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_d) {
			right.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_w) {
			up.pressed = false;
			return true;
		} else if (evt.key.keysym.sym == SDLK_s) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_MOUSEBUTTONDOWN) {
		if (SDL_GetRelativeMouseMode() == SDL_FALSE) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			return true;
		}
	} else if (evt.type == SDL_MOUSEMOTION) {
		if (SDL_GetRelativeMouseMode() == SDL_TRUE) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			
			static constexpr float camera_speed = 1.0f;

			cam_x = glm::clamp(cam_x + camera_speed * motion.y * camera->fovy, x_default - x_range, x_default + x_range);
			cam_z = glm::clamp(cam_z - camera_speed * motion.x * camera->fovy, z_default - z_range, z_default + z_range);
			camera->transform->rotation = glm::quat(glm::vec3(cam_x, 0.0f, cam_z));
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	if (!alive) return;

	time_survived += elapsed;

	static std::mt19937 mt = std::mt19937(time(0));

	timer += elapsed;
	while (timer >= 0.1f) {
		timer -= 0.1f;

		size_t i = mt() % (pillar_width * pillar_width);
		if (pillar_animation_time[i] == 0.0f) {
			pillar_animation_time[i] = 0.0001f;
		}

	}

	if (cube_animation_time > 0.0f) {
		cube_animation_time += elapsed;
	}

	if (cube_animation_time > CUBE_ANIM_TIME) {
		cube_animation_time = 0.0f;
		player_tile_old = player_tile_new;
	}

	for (float &f : pillar_animation_time) {
		if (f > 0.0f) {
			f += elapsed;
		}

		if (f > PILLAR_ANIM_TIME) {
			f = 0.0f;
		}
	}


	// update transformation matrices
	for (size_t i = 0; i < pillar_width * pillar_width; i++) {
		float anim_time = pillar_animation_time[i];
		Scene::Transform *t = pillars[i];

		// https://gist.github.com/companje/29408948f1e8be54dd5733a74ca49bb9
		// from companje on github
		static auto map = [](float val, float x1, float x2, float y1, float y2) -> float {
			return y1 + (val - x1) * (y2 - y1) / (x2 - x1);
		};

		if (anim_time < PILLAR_DROP_TIME) {
			t->position.z = 0.0f;
			color_lerp_for_shader_uniform[i] = map(anim_time, 0, PILLAR_DROP_TIME, 0.0f, 0.75f);
		} else if (anim_time < PILLAR_DONE_TIME) {
			t->position.z = map(anim_time, PILLAR_DROP_TIME, PILLAR_DONE_TIME, 0.0f, PILLAR_MIN_Z);
			color_lerp_for_shader_uniform[i] = 0.75f;
		} else if (anim_time < PILLAR_RISE_TIME) {
			t->position.z = PILLAR_MIN_Z;
			color_lerp_for_shader_uniform[i] = 0.75f;
		} else {
			t->position.z = map(anim_time, PILLAR_RISE_TIME, PILLAR_ANIM_TIME, PILLAR_MIN_Z, 0.0f);
			color_lerp_for_shader_uniform[i] = map(anim_time, PILLAR_RISE_TIME, PILLAR_ANIM_TIME, 0.75f, 1.0f);
		}

		
	}

	glm::vec3 cube_pos_old;
	glm::vec3 cube_pos_new;
	cube_pos_old = glm::vec3(
			(player_tile_old.x - (int)(pillar_width / 2)) * pillar_distance, 
			(player_tile_old.y - (int)(pillar_width / 2)) * pillar_distance, 
			pillars[player_tile_old.y * pillar_width + player_tile_old.x]->position.z + 1.0f);

	cube_pos_new = glm::vec3(
			(player_tile_new.x - (int)(pillar_width / 2)) * pillar_distance, 
			(player_tile_new.y - (int)(pillar_width / 2)) * pillar_distance, 
			pillars[player_tile_new.y * pillar_width + player_tile_new.x]->position.z + 1.0f);

	cube->position = glm::mix(cube_pos_old, cube_pos_new, cube_animation_time/CUBE_ANIM_TIME);
	if (cube->position.z < -4.0f) {
		alive = false;
	}

	//reset button press counters:
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for custom_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(custom_texture_program->program);
	glUniform1i(custom_texture_program->LIGHT_TYPE_int, 1);
	GL_ERRORS();
	glUniform3fv(custom_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	GL_ERRORS();
	glUniform3fv(custom_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	GL_ERRORS();
	glUseProgram(0);

	glClearColor(0.5f, 0.9f, 1.0f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.07f;
		lines.draw_text("Mouse for camera; WASD to move; Escape ungrabs mouse; R restarts",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Mouse for camera; WASD to move; Escape ungrabs mouse; R restarts",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		lines.draw_text("Time survived: " + std::to_string((int)time_survived),
			glm::vec3(-aspect + 0.1f * H, 1.0f - 1.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
	}
}
