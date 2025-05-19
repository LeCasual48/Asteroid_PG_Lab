#include <vector>
#include <algorithm>
#include <functional> 
#include <memory>
#include <cstdlib>
#include <cmath>
#include <ctime>

#include <raylib.h>
#include <raymath.h>

// --- UTILS ---
namespace Utils {
	inline static float RandomFloat(float min, float max) {
		return min + static_cast<float>(rand()) / RAND_MAX * (max - min);
	}
}

// --- TRANSFORM, PHYSICS, LIFETIME, RENDERABLE ---
struct TransformA {
	Vector2 position{};
	float rotation{};
};

struct Physics {
	Vector2 velocity{};
	float rotationSpeed{};
};

struct Renderable {
	enum Size { SMALL = 1, MEDIUM = 2, LARGE = 4 } size = SMALL;
};

// --- RENDERER ---
class Renderer {
public:
	static Renderer& Instance() {
		static Renderer inst;
		return inst;
	}

	void Init(int w, int h, const char* title) {
		InitWindow(w, h, title);
		SetTargetFPS(60);
		screenW = w;
		screenH = h;
	}

	void Begin() {
		BeginDrawing();
		ClearBackground(BLACK);
	}

	void End() {
		EndDrawing();
	}

	void DrawPoly(const Vector2& pos, int sides, float radius, float rot) {
		DrawPolyLines(pos, sides, radius, rot, WHITE);
	}

	int Width() const {
		return screenW;
	}

	int Height() const {
		return screenH;
	}

private:
	Renderer() = default;

	int screenW{};
	int screenH{};
};

// --- ASTEROID HIERARCHY ---

class Asteroid {
public:
	Asteroid(int screenW, int screenH) {
		init(screenW, screenH);
	}
	virtual ~Asteroid() = default;

	bool Update(float dt) {
		transform.position = Vector2Add(transform.position, Vector2Scale(physics.velocity, dt));
		transform.rotation += physics.rotationSpeed * dt;
		if (transform.position.x < -GetRadius() || transform.position.x > Renderer::Instance().Width() + GetRadius() ||
			transform.position.y < -GetRadius() || transform.position.y > Renderer::Instance().Height() + GetRadius())
			return false;
		return true;
	}
	virtual void Draw() const = 0;

	Vector2 GetPosition() const {
		return transform.position;
	}

	float constexpr GetRadius() const {
		return 16.f * (float)render.size;
	}

	int GetDamage() const {
		return baseDamage * static_cast<int>(render.size);
	}

	int GetSize() const {
		return static_cast<int>(render.size);
	}

	bool Shrink() {
		if (render.size == Renderable::SMALL) {
			return true; // usunąć asteroidę
		}
		if (render.size == Renderable::LARGE) {
			render.size = Renderable::MEDIUM;
		} else if (render.size == Renderable::MEDIUM) {
			render.size = Renderable::SMALL;
		}
		return false; // nie usuwać jeszcze
	}

protected:
	void init(int screenW, int screenH) {
		// Choose size
		render.size = static_cast<Renderable::Size>(1 << GetRandomValue(0, 2));

		// Spawn at random edge
		switch (GetRandomValue(0, 3)) {
		case 0:
			transform.position = { Utils::RandomFloat(0, screenW), -GetRadius() };
			break;
		case 1:
			transform.position = { screenW + GetRadius(), Utils::RandomFloat(0, screenH) };
			break;
		case 2:
			transform.position = { Utils::RandomFloat(0, screenW), screenH + GetRadius() };
			break;
		default:
			transform.position = { -GetRadius(), Utils::RandomFloat(0, screenH) };
			break;
		}

		// Aim towards center with jitter
		float maxOff = fminf(screenW, screenH) * 0.1f;
		float ang = Utils::RandomFloat(0, 2 * PI);
		float rad = Utils::RandomFloat(0, maxOff);
		Vector2 center = {
			screenW * 0.5f + cosf(ang) * rad,
			screenH * 0.5f + sinf(ang) * rad
		};

		Vector2 dir = Vector2Normalize(Vector2Subtract(center, transform.position));
		physics.velocity = Vector2Scale(dir, Utils::RandomFloat(SPEED_MIN, SPEED_MAX));
		physics.rotationSpeed = Utils::RandomFloat(ROT_MIN, ROT_MAX);

		transform.rotation = Utils::RandomFloat(0, 360);
	}

	TransformA transform;
	Physics    physics;
	Renderable render;

	int baseDamage = 0;
	static constexpr float LIFE = 10.f;
	static constexpr float SPEED_MIN = 125.f;
	static constexpr float SPEED_MAX = 250.f;
	static constexpr float ROT_MIN = 50.f;
	static constexpr float ROT_MAX = 240.f;
};

class TriangleAsteroid : public Asteroid {
public:
	TriangleAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 5; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 3, GetRadius(), transform.rotation);
	}
};
class SquareAsteroid : public Asteroid {
public:
	SquareAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 10; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 4, GetRadius(), transform.rotation);
	}
};
class PentagonAsteroid : public Asteroid {
public:
	PentagonAsteroid(int w, int h) : Asteroid(w, h) { baseDamage = 15; }
	void Draw() const override {
		Renderer::Instance().DrawPoly(transform.position, 5, GetRadius(), transform.rotation);
	}
};

// Shape selector
enum class AsteroidShape { TRIANGLE = 3, SQUARE = 4, PENTAGON = 5, RANDOM = 0 };

// Factory
static inline std::unique_ptr<Asteroid> MakeAsteroid(int w, int h, AsteroidShape shape) {
	switch (shape) {
	case AsteroidShape::TRIANGLE:
		return std::make_unique<TriangleAsteroid>(w, h);
	case AsteroidShape::SQUARE:
		return std::make_unique<SquareAsteroid>(w, h);
	case AsteroidShape::PENTAGON:
		return std::make_unique<PentagonAsteroid>(w, h);
	default: {
		return MakeAsteroid(w, h, static_cast<AsteroidShape>(3 + GetRandomValue(0, 2)));
	}
	}
}

// --- PROJECTILE HIERARCHY ---
enum class WeaponType { LASER, BULLET, COUNT };
class Projectile {
public:
	Projectile(Vector2 pos, Vector2 vel, int dmg, WeaponType wt)
	{
		transform.position = pos;
		physics.velocity = vel;
		baseDamage = dmg;
		type = wt;
	}
	bool Update(float dt) {
		transform.position = Vector2Add(transform.position, Vector2Scale(physics.velocity, dt));

		if (transform.position.x < 0 ||
			transform.position.x > Renderer::Instance().Width() ||
			transform.position.y < 0 ||
			transform.position.y > Renderer::Instance().Height())
		{
			return true;
		}
		return false;
	}
	void Draw() const {
		if (type == WeaponType::BULLET) {
			DrawCircleV(transform.position, 5.f, WHITE);
		}
		else {
			static constexpr float LASER_LENGTH = 30.f;
			Rectangle lr = { transform.position.x - 2.f, transform.position.y - LASER_LENGTH, 4.f, LASER_LENGTH };
			DrawRectangleRec(lr, RED);
		}
	}
	Vector2 GetPosition() const {
		return transform.position;
	}

	float GetRadius() const {
		return (type == WeaponType::BULLET) ? 5.f : 2.f;
	}

	int GetDamage() const {
		return baseDamage;
	}

private:
	TransformA transform;
	Physics    physics;
	int        baseDamage;
	WeaponType type;
};

inline static Projectile MakeProjectile(WeaponType wt,
	const Vector2 pos,
	float speed)
{
	Vector2 vel{ 0, -speed };
	if (wt == WeaponType::LASER) {
		return Projectile(pos, vel, 20, wt);
	}
	else {
		return Projectile(pos, vel, 10, wt);
	}
}

// --- SHIP HIERARCHY ---
class Ship {
public:
	Ship(int screenW, int screenH) {
		transform.position = {
			screenW * 0.5f,
			screenH * 0.5f
		};
		hp = 100;
		speed = 250.f;
		alive = true;

		// per-weapon fire rate & spacing
		fireRateLaser = 18.f; // shots/sec
		fireRateBullet = 22.f;
		spacingLaser = 40.f; // px between lasers
		spacingBullet = 20.f;
	}
	virtual ~Ship() = default;
	virtual void Update(float dt) = 0;
	virtual void Draw() const = 0;

	void TakeDamage(int dmg) {
		if (!alive) return;
		hp -= dmg;
		if (hp <= 0) alive = false;
	}

	bool IsAlive() const {
		return alive;
	}

	Vector2 GetPosition() const {
		return transform.position;
	}

	virtual float GetRadius() const = 0;

	int GetHP() const {
		return hp;
	}

	float GetFireRate(WeaponType wt) const {
		return (wt == WeaponType::LASER) ? fireRateLaser : fireRateBullet;
	}

	float GetSpacing(WeaponType wt) const {
		return (wt == WeaponType::LASER) ? spacingLaser : spacingBullet;
	}

protected:
	TransformA transform;
	int        hp;
	float      speed;
	bool       alive;
	float      fireRateLaser;
	float      fireRateBullet;
	float      spacingLaser;
	float      spacingBullet;
};

class PlayerShip :public Ship {
public:
	PlayerShip(int w, int h) : Ship(w, h) {
		texture = LoadTexture("placek.png");
		GenTextureMipmaps(&texture);
		SetTextureFilter(texture, 2);
		scale = 0.25f;
	}
	~PlayerShip() {
		UnloadTexture(texture);
	}

	void Update(float dt) override {
		if (alive) {
			if (IsKeyDown(KEY_W)) transform.position.y -= speed * dt;
			if (IsKeyDown(KEY_S)) transform.position.y += speed * dt;
			if (IsKeyDown(KEY_A)) transform.position.x -= speed * dt;
			if (IsKeyDown(KEY_D)) transform.position.x += speed * dt;
		}
		else {
			transform.position.y += speed * dt;
		}
	}

	void Draw() const override {
		if (!alive && fmodf(GetTime(), 0.4f) > 0.2f) return;
		Vector2 dstPos = {
			transform.position.x - (texture.width * scale) * 0.5f,
			transform.position.y - (texture.height * scale) * 0.5f
		};
		DrawTextureEx(texture, dstPos, 0.0f, scale, WHITE);
	}

	float GetRadius() const override {
		return (texture.width * scale) * 0.5f;
	}

private:
	Texture2D texture;
	float     scale;
};

// --- APPLICATION ---
class Application {
public:
	static Application& Instance() {
		static Application inst;
		return inst;
	}

	void Run() {
		srand(static_cast<unsigned>(time(nullptr)));
		Renderer::Instance().Init(C_WIDTH, C_HEIGHT, "Asteroids OOP");

		auto player = std::make_unique<PlayerShip>(C_WIDTH, C_HEIGHT);

		int ammo[(int)WeaponType::COUNT] = { 50, 50 };
		const int maxAmmo[(int)WeaponType::COUNT] = { 50, 50 };
		int score = 0;

		float spawnTimer = 0.f;
		float spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
		WeaponType currentWeapon = WeaponType::LASER;
		float shotTimer = 0.f;

		bool gameOver = false;

		while (!WindowShouldClose()) {
			float dt = GetFrameTime();
			spawnTimer += dt;

			// --- GAME LOGIC ---
			if (!gameOver) {
				player->Update(dt);

				// Regeneracja amunicji
				for (int i = 0; i < (int)WeaponType::COUNT; ++i) {
					if (ammo[i] < maxAmmo[i]) {
						ammo[i] += (int)(dt * 5);
						if (ammo[i] > maxAmmo[i]) ammo[i] = maxAmmo[i];
					}
				}

				// Restart logic
				if (!player->IsAlive()) {
					gameOver = true;
				}

				if (IsKeyPressed(KEY_ONE)) currentShape = AsteroidShape::TRIANGLE;
				if (IsKeyPressed(KEY_TWO)) currentShape = AsteroidShape::SQUARE;
				if (IsKeyPressed(KEY_THREE)) currentShape = AsteroidShape::PENTAGON;
				if (IsKeyPressed(KEY_FOUR)) currentShape = AsteroidShape::RANDOM;

				if (IsKeyPressed(KEY_TAB)) {
					currentWeapon = static_cast<WeaponType>((static_cast<int>(currentWeapon) + 1) % static_cast<int>(WeaponType::COUNT));
				}

				// Shooting
				if (player->IsAlive() && IsKeyDown(KEY_SPACE) && ammo[(int)currentWeapon] > 0) {
					shotTimer += dt;
					float interval = 1.f / player->GetFireRate(currentWeapon);
					float projSpeed = player->GetSpacing(currentWeapon) * player->GetFireRate(currentWeapon);

					while (shotTimer >= interval && ammo[(int)currentWeapon] > 0) {
						Vector2 p = player->GetPosition();
						p.y -= player->GetRadius();
						projectiles.push_back(MakeProjectile(currentWeapon, p, projSpeed));
						shotTimer -= interval;
						ammo[(int)currentWeapon]--;
					}
				}
				else {
					float maxInterval = 1.f / player->GetFireRate(currentWeapon);
					if (shotTimer > maxInterval) {
						shotTimer = fmodf(shotTimer, maxInterval);
					}
				}

				// Spawn asteroids
				if (spawnTimer >= spawnInterval && asteroids.size() < MAX_AST) {
					asteroids.push_back(MakeAsteroid(C_WIDTH, C_HEIGHT, currentShape));
					spawnTimer = 0.f;
					spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
				}

				// Update projectiles
				{
					auto projectile_to_remove = std::remove_if(projectiles.begin(), projectiles.end(),
						[dt](auto& projectile) {
							return projectile.Update(dt);
						});
					projectiles.erase(projectile_to_remove, projectiles.end());
				}

				// Projectile-Asteroid collisions
				for (auto pit = projectiles.begin(); pit != projectiles.end();) {
					bool removed = false;
					for (auto ait = asteroids.begin(); ait != asteroids.end(); ++ait) {
						float dist = Vector2Distance((*pit).GetPosition(), (*ait)->GetPosition());
						if (dist < (*pit).GetRadius() + (*ait)->GetRadius()) {
							if ((*ait)->Shrink()) {
								ait = asteroids.erase(ait);
								score += 10;
								ammo[(int)currentWeapon] += 10;
								if (ammo[(int)currentWeapon] > maxAmmo[(int)currentWeapon])
									ammo[(int)currentWeapon] = maxAmmo[(int)currentWeapon];
							}
							else {
								++ait;
							}
							pit = projectiles.erase(pit);
							removed = true;
							break;
						}
					}
					if (!removed) {
						++pit;
					}
				}

				// Asteroid-Ship collisions
				{
					auto remove_collision =
						[&player, dt](auto& asteroid_ptr_like) -> bool {
							if (player->IsAlive()) {
								float dist = Vector2Distance(player->GetPosition(), asteroid_ptr_like->GetPosition());
								if (dist < player->GetRadius() + asteroid_ptr_like->GetRadius()) {
									player->TakeDamage(asteroid_ptr_like->GetDamage());
									return true;
								}
							}
							if (!asteroid_ptr_like->Update(dt)) {
								return true;
							}
							return false;
						};
					auto asteroid_to_remove = std::remove_if(asteroids.begin(), asteroids.end(), remove_collision);
					asteroids.erase(asteroid_to_remove, asteroids.end());
				}
			} // end !gameOver

			// --- RENDER ---
			Renderer::Instance().Begin();

			if (!gameOver) {
				// Pasek życia
				int maxHP = 100;
				int barWidth = 300;
				int barHeight = 24;
				int hp = player->GetHP();
				float percent = (float)hp / maxHP;
				DrawRectangle(10, 10, barWidth, barHeight, DARKGRAY);
				DrawRectangle(10, 10, (int)(barWidth * percent), barHeight, GREEN);
				DrawRectangleLines(10, 10, barWidth, barHeight, WHITE);

				DrawText(TextFormat("HP: %d", hp), 20, 14, 18, WHITE);

				const char* weaponName = (currentWeapon == WeaponType::LASER) ? "LASER" : "BULLET";
				DrawText(TextFormat("Weapon: %s", weaponName), 10, 40, 20, BLUE);

				DrawText(TextFormat("Ammo LASER: %d/%d", ammo[(int)WeaponType::LASER], maxAmmo[(int)WeaponType::LASER]), 10, 70, 20, YELLOW);
				DrawText(TextFormat("Ammo BULLET: %d/%d", ammo[(int)WeaponType::BULLET], maxAmmo[(int)WeaponType::BULLET]), 10, 100, 20, YELLOW);

				DrawText(TextFormat("Score: %d", score), 10, 130, 20, ORANGE);

				for (const auto& projPtr : projectiles) {
					projPtr.Draw();
				}
				for (const auto& astPtr : asteroids) {
					astPtr->Draw();
				}

				player->Draw();
			} else {
				// Ekran końcowy
				const char* gameOverText = "GAME OVER";
				const char* restartText = "Nacisnij R aby zrestartowac";
				int fontSize = 60;
				int textWidth = MeasureText(gameOverText, fontSize);
				int restartWidth = MeasureText(restartText, 30);

				DrawText(gameOverText, (C_WIDTH - textWidth) / 2, C_HEIGHT / 2 - 80, fontSize, RED);
				DrawText(TextFormat("Wynik: %d", score), (C_WIDTH - 200) / 2, C_HEIGHT / 2, 40, ORANGE);
				DrawText(restartText, (C_WIDTH - restartWidth) / 2, C_HEIGHT / 2 + 60, 30, WHITE);

				// Restart gry
				if (IsKeyPressed(KEY_R)) {
					player = std::make_unique<PlayerShip>(C_WIDTH, C_HEIGHT);
					asteroids.clear();
					projectiles.clear();
					spawnTimer = 0.f;
					spawnInterval = Utils::RandomFloat(C_SPAWN_MIN, C_SPAWN_MAX);
					for (int i = 0; i < (int)WeaponType::COUNT; ++i) ammo[i] = maxAmmo[i];
					score = 0;
					currentShape = AsteroidShape::RANDOM;
					gameOver = false;
				}
			}

			Renderer::Instance().End();
		}
	}

private:
	Application()
	{
		asteroids.reserve(1000);
		projectiles.reserve(10'000);
	};

	std::vector<std::unique_ptr<Asteroid>> asteroids;
	std::vector<Projectile> projectiles;

	AsteroidShape currentShape = AsteroidShape::RANDOM;

	static constexpr int C_WIDTH = 1000;
	static constexpr int C_HEIGHT = 1000;
	static constexpr size_t MAX_AST = 150;
	static constexpr float C_SPAWN_MIN = 0.5f;
	static constexpr float C_SPAWN_MAX = 3.0f;

	static constexpr int C_MAX_ASTEROIDS = 1000;
	static constexpr int C_MAX_PROJECTILES = 10'000;
};

int main() {
	Application::Instance().Run();
	return 0;
}
