#include <algorithm>
#include <atomic>
#include <chrono>
#define _USE_MATH_DEFINES
#include <cmath>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <thread>

using namespace ftxui;

std::vector<std::string> world_map = {"................", "................",
                                      "................", "................",
                                      "................", "................",
                                      "................", "................",
                                      "................", "................",
                                      "..........XX....",  // Airport top row
                                      "..........XX....",  // Airport bottom row
                                      "................", "................"};

struct Plane {
  double altitude = 2000;
  double speed = 250;
  double pitch = 0;
  double roll = 0;
  double throttle = 0.6;
  double fuel = 1.0;
  double destination_distance = 10000.0;
  bool gear_down = false;
  bool flaps = false;
  bool crashed = false;
  bool landed = false;

  double map_x = 3.0;
  double map_y = 8.0;
  double heading = 0.0;

  int dest_x = 10;
  int dest_y = 10;

  void update(double dt) {
    if (crashed || landed)
      return;

    double lift = std::max(0.0, (speed / 200.0) * (1.0 + pitch / 10.0));
    double sink = 50.0 - lift * 30.0 + std::abs(roll) * 0.5;

    altitude += (pitch * 3.0 - sink) * dt;
    altitude = std::max(0.0, altitude);

    speed += (throttle - 0.5) * 50.0 * dt;
    speed = std::clamp(speed, 80.0, 350.0);

    fuel -= throttle * 0.007 * dt;
    fuel = std::max(0.0, fuel);

    double speed_ms = speed * 0.5144;
    destination_distance =
        std::max(-400.0, destination_distance - speed_ms * dt);

    double turn_rate = tan(roll * M_PI / 180.0) * 0.4;
    heading += turn_rate * dt;

    double MAP_SCALE = 0.001;
    double new_x = map_x + cos(heading) * speed_ms * dt * MAP_SCALE;
    double new_y = map_y + sin(heading) * speed_ms * dt * MAP_SCALE;

    if (new_y >= 0 && new_y < world_map.size() && new_x >= 0 &&
        new_x < world_map[0].size()) {
      map_x = new_x;
      map_y = new_y;
    }

    if (altitude <= 0.0) {
      bool on_runway = ((int)map_x >= dest_x && (int)map_x <= dest_x + 1 &&
                        (int)map_y >= dest_y && (int)map_y <= dest_y + 1);

      if (on_runway && speed < 140 && pitch > -5 && gear_down &&
          std::abs(roll) < 10) {
        landed = true;
      } else {
        crashed = true;
      }
      return;
    }
  }
};

Color GetSpeedColor(double s) {
  if (s < 120)
    return Color::Red;
  if (s < 180)
    return Color::Green;
  if (s < 250)
    return Color::Yellow;
  return Color::Red;
}

Color GetAltitudeColor(double a) {
  if (a < 100)
    return Color::Red;
  if (a < 300)
    return Color::Yellow;
  return Color::Green;
}

Color GetFuelColor(double f) {
  if (f < 0.1)
    return Color::Red;
  if (f < 0.3)
    return Color::Yellow;
  return Color::Green;
}

// =====================================================================
// ATTITUDE INDICATOR
// =====================================================================
auto render_horizon = [&](double pitch, double roll) -> Element {
  const int rows = 9, cols = 25;
  std::vector<std::vector<Color>> cell_color(
      rows, std::vector<Color>(cols, Color::Black));

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      double dx = x - cols / 2.0;
      double dy = y - rows / 2.0;
      double rr = roll * M_PI / 180.0;
      double rotated_y = dy * cos(rr) - dx * sin(rr);

      rotated_y -= pitch / 5.0;

      if (std::abs(rotated_y) < 0.3)
        cell_color[y][x] = Color::Yellow;
      else if (rotated_y < -0.3)
        cell_color[y][x] = Color::RGB(100, 180, 255);
      else if (rotated_y < 1.0)
        cell_color[y][x] = Color::RGB(220, 220, 220);
      else
        cell_color[y][x] = Color::RGB(160, 90, 45);
    }
  }

  std::vector<Element> lines;
  for (int y = 0; y < rows; y++) {
    std::vector<Element> row;
    for (int x = 0; x < cols; x++)
      row.push_back(text(" ") | bgcolor(cell_color[y][x]));
    lines.push_back(hbox(row));
  }
  return vbox(lines) | borderRounded;
};

// =====================================================================
// RADAR
// =====================================================================
auto render_radar = [&](double sweep_angle,
                        double destination_distance) -> Element {
  const int width = 22, height = 11;
  const int cx = width / 2, cy = height / 2;

  std::vector<std::vector<Color>> grid(
      height, std::vector<Color>(width, Color::RGB(0, 20, 0)));

  for (int y = 0; y < height; y++)
    for (int x = 0; x < width; x++) {
      double dx = (x - cx) / 1.6;
      double dy = (y - cy);
      double dist = sqrt(dx * dx + dy * dy);
      double radius = std::min(cx, cy) - 0.5;

      if (dist > radius - 0.5 && dist < radius + 0.5)
        grid[y][x] = Color::RGB(0, 100, 0);
    }

  double rad = sweep_angle * M_PI / 180.0;
  for (int r = 0; r < std::min(cx, cy); r++) {
    int sx = (int)(cx + cos(rad) * r * 1.6);
    int sy = (int)(cy - sin(rad) * r);
    if (sx >= 0 && sx < width && sy >= 0 && sy < height)
      grid[sy][sx] = Color::LightGreen;
  }

  std::vector<Element> lines;
  for (int y = 0; y < height; y++) {
    std::vector<Element> row;
    for (int x = 0; x < width; x++)
      row.push_back(text(" ") | bgcolor(grid[y][x]));
    lines.push_back(hbox(row));
  }

  return vbox({
             text(" RADAR ") | bold | color(Color::LightGreen) | center,
             vbox(lines) | borderRounded,
         }) |
         center;
};

Element render_map(const Plane& plane) {
  std::vector<Element> rows;

  // heading → arrow
  double deg = fmod((plane.heading * 180.0 / M_PI) + 360.0, 360.0);

  std::string icon = "→";
  if (deg < 22.5)
    icon = "→";
  else if (deg < 67.5)
    icon = "↘";
  else if (deg < 112.5)
    icon = "↓";
  else if (deg < 157.5)
    icon = "↙";
  else if (deg < 202.5)
    icon = "←";
  else if (deg < 247.5)
    icon = "↖";
  else if (deg < 292.5)
    icon = "↑";
  else
    icon = "↗";

  for (int y = 0; y < world_map.size(); y++) {
    std::vector<Element> row;
    for (int x = 0; x < world_map[y].size(); x++) {
      if ((int)plane.map_y == y && (int)plane.map_x == x) {
        row.push_back(text(icon) | color(Color::Yellow));
      }

      // Airport 2x2
      else if (x >= plane.dest_x && x <= plane.dest_x + 1 &&
               y >= plane.dest_y && y <= plane.dest_y + 1) {
        row.push_back(text("X") | color(Color::Blue) | bold);
      }

      else {
        row.push_back(text(std::string(1, world_map[y][x])));
      }
    }
    rows.push_back(hbox(row));
  }

  return vbox(rows) | borderRounded | color(Color::Green);
}

// =====================================================================
// MAIN UI
// =====================================================================
int main() {
  auto screen = ScreenInteractive::Fullscreen();
  Plane plane;
  std::atomic<bool> running = true;

  // Buttons
  Component gear_button =
      Button("Toggle Gear", [&] { plane.gear_down = !plane.gear_down; });
  Component flaps_button =
      Button("Toggle Flaps", [&] { plane.flaps = !plane.flaps; });
  auto buttons = Container::Horizontal({gear_button, flaps_button});

  // =================================================================
  //  Cockpit Renderer
  // =================================================================
  auto cockpit_renderer = Renderer(buttons, [&] {
    std::string status = "FLYING";
    Color status_col = Color::Green;

    if (plane.landed) {
      status = "LANDED SAFELY";
      status_col = Color::Green;
    } else if (plane.crashed) {
      status = "CRASHED";
      status_col = Color::Red;
    }

    auto bar = [](double v, int w, Color c) {
      int f = (int)(v * w);
      std::string s(f, '|');
      s += std::string(w - f, '-');
      return text("[" + s + "]") | color(c);
    };
    auto panel = [&](const std::string& l, const std::string& v, Color c) {
      return hbox({text(l + ": ") | bold, text(v) | color(c)});
    };

    return vbox({
               text("TERMINAL FLIGHT SIMULATOR") | bold | color(Color::Cyan) |
                   center,
               separator(),

               hbox({
                   border(vbox({
                       panel("ALT", std::to_string((int)plane.altitude) + " ft",
                             GetAltitudeColor(plane.altitude)),
                       panel("SPD", std::to_string((int)plane.speed) + " kt",
                             GetSpeedColor(plane.speed)),
                       panel("PITCH", std::to_string((int)plane.pitch) + "°",
                             Color::White),
                       panel("ROLL", std::to_string((int)plane.roll) + "°",
                             Color::White),
                   })),

                   border(vbox({
                       panel("THR",
                             std::to_string((int)(plane.throttle * 100)) + "%",
                             Color::Cyan),
                       bar(plane.throttle, 20, Color::Cyan),

                       panel("FUEL",
                             std::to_string((int)(plane.fuel * 100)) + "%",
                             GetFuelColor(plane.fuel)),
                       bar(plane.fuel, 20, GetFuelColor(plane.fuel)),
                   })),
               }) | center,
               separator(),

               hbox({
                   vbox({
                       text("Attitude") | bold | color(Color::Yellow) | center,
                       render_horizon(plane.pitch, plane.roll),
                   }) | flex,

                   render_radar(fmod((double)std::chrono::duration_cast<
                                         std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now()
                                             .time_since_epoch())
                                             .count() /
                                         20.0,
                                     360.0),
                                plane.destination_distance),

                   vbox({
                       text("MAP (1 km/tile)") | bold | color(Color::Green) |
                           center,
                       render_map(plane),
                   }),
               }) | center,

               separator(),

               hbox({
                   text("GEAR: ") | bold,
                   text(plane.gear_down ? "DOWN" : "UP") |
                       color(plane.gear_down ? Color::Green : Color::Red),
                   gear_button->Render() | border | color(Color::Cyan),

                   text("   FLAPS: ") | bold,
                   text(plane.flaps ? "ON" : "OFF") |
                       color(plane.flaps ? Color::Green : Color::GrayLight),
                   flaps_button->Render() | border | color(Color::Cyan),
               }) | center,

               separator(),
               text("STATUS: " + status) | bold | color(status_col) | center,

               separator(),
               text("Controls: ↑↓ Pitch  ←→ Roll  A/D Throttle  G Gear  F "
                    "Flaps  Q Quit") |
                   dim | center,
           }) |
           borderRounded | bgcolor(Color::Black);
  });

  cockpit_renderer |= CatchEvent([&](Event e) {
    if (e.is_character()) {
      char c = e.character()[0];
      switch (c) {
        case 'q':
          running = false;
          screen.ExitLoopClosure()();
          break;
        case 'a':
          plane.throttle = std::max(plane.throttle - 0.05, 0.0);
          break;
        case 'd':
          plane.throttle = std::min(plane.throttle + 0.05, 1.0);
          break;
        case 'g':
          plane.gear_down = !plane.gear_down;
          break;
        case 'f':
          plane.flaps = !plane.flaps;
          break;
      }
    }

    if (e == Event::ArrowUp)
      plane.pitch = std::max(plane.pitch - 1.0, -15.0);
    if (e == Event::ArrowDown)
      plane.pitch = std::min(plane.pitch + 1.0, 15.0);
    if (e == Event::ArrowLeft)
      plane.roll = std::max(plane.roll - 2.0, -45.0);
    if (e == Event::ArrowRight)
      plane.roll = std::min(plane.roll + 2.0, 45.0);

    return false;
  });

  std::thread logic([&] {
    using namespace std::chrono;
    auto last = steady_clock::now();
    while (running) {
      auto now = steady_clock::now();
      double dt = duration<double>(now - last).count();
      last = now;

      plane.update(dt);
      screen.PostEvent(Event::Custom);

      std::this_thread::sleep_for(50ms);
    }
  });

  screen.Loop(cockpit_renderer);
  running = false;
  logic.join();
  return 0;
}
