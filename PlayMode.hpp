#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <array>
#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	float time_survived = 0.0f;
	bool alive = true;
	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// camera movement
	static constexpr float x_default = 1.3f;
	static constexpr float x_range = 1.0f;

	static constexpr float z_default = 1.570796f;
	static constexpr float z_range = 1.0f;

	float cam_x = x_default;
	float cam_z = z_default;

	// 81 pillar transforms
	static constexpr size_t pillar_width = 9;
	std::array<Scene::Transform *, pillar_width * pillar_width> pillars;

	static constexpr float pillar_distance = 3.0f;

	// cube transform
	Scene::Transform *cube;
	float cube_animation_time;

	glm::ivec2 player_tile_old;
	glm::ivec2 player_tile_new;
	glm::vec3  player_rot_axis;
	glm::quat  player_rot;

	static constexpr float CUBE_ANIM_TIME = 0.5f;

	// to drop out a pillar, we start the "pillar_animation_time" variable.
	// this starts out at zero, and goes to ANIM_TIME.
	std::array<float, 81> pillar_animation_time;
	std::array<float, 81> color_lerp_for_shader_uniform;	
	// keyframes
	static constexpr float PILLAR_DROP_TIME = 7.0f;
	static constexpr float PILLAR_DONE_TIME = 7.5f;
	static constexpr float PILLAR_RISE_TIME = 9.5f;
	static constexpr float PILLAR_ANIM_TIME = 10.0f;

	static constexpr float PILLAR_MIN_Z = -30.0f;
	glm::mat4x3 PILLAR_COLORS = glm::mat4x3(
		glm::vec3(1.0f, 1.0f, 1.0f),
		glm::vec3(1.0f, 1.0f, 0.0f),
		glm::vec3(1.0f, 0.0f, 1.0f),
		glm::vec3(0.0f, 1.0f, 1.0f)
	);

	float timer;

	//camera:
	Scene::Camera *camera = nullptr;

};
