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

std::vector<std::string> world_map = {
    "................", "................", "................",
    "................", "................", "................",
    "................", "................", "................",
    "................", "..........XX....", "..........XX....",
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

  double map_x = 10.0;  
  double map_y = 3.0;   
  double heading = M_PI / 2.0;

  int dest_x = 10;
  int dest_y = 10;

  void update(double dt) {
    if (crashed || landed)
      return;

    if (fuel <= 0.0) {
      throttle = 0.0;
    }

    double lift = std::max(0.0, (speed / 200.0) * (1.0 + pitch / 10.0));
    double sink = 50.0 - lift * 30.0 + std::abs(roll) * 0.5;

    altitude += (pitch * 3.0 - sink) * dt;
    
    if (altitude <= 0.0) {
      altitude = 0.0;
      
      bool on_runway = ((int)map_x >= dest_x && (int)map_x <= dest_x + 1 &&
                        (int)map_y >= dest_y && (int)map_y <= dest_y + 1);

      if (on_runway && speed < 140 && pitch > -5 && gear_down &&
          std::abs(roll) < 10) {
        landed = true;
        speed = 0;
      } else {
        crashed = true;
        speed = 0;
      }
      return;
    }

    speed += (throttle - 0.5) * 50.0 * dt;
    speed = std::clamp(speed, 80.0, 350.0);

    fuel -= throttle * 0.001 * dt;
    fuel = std::max(0.0, fuel);

    double speed_ms = speed * 0.5144;
    destination_distance =
        std::max(-400.0, destination_distance - speed_ms * dt);

    double turn_rate = tan(roll * M_PI / 180.0) * 0.4;
    heading += turn_rate * dt;

    double MAP_SCALE = 0.001;
    double new_x = map_x + sin(heading) * speed_ms * dt * MAP_SCALE;
    double new_y = map_y - cos(heading) * speed_ms * dt * MAP_SCALE;

    if (new_y >= 0 && new_y < world_map.size() && new_x >= 0 &&
        new_x < world_map[0].size()) {
      map_x = new_x;
      map_y = new_y;
    }
  }
};

auto render_artificial_horizon = [](double pitch, double roll) -> Element {
  const int rows = 13, cols = 27;
  std::vector<std::vector<Color>> cell_color(
      rows, std::vector<Color>(cols, Color::Black));

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      double dx = x - cols / 2.0;
      double dy = y - rows / 2.0;
      double rr = roll * M_PI / 180.0;
      double rotated_y = dy * cos(rr) - dx * sin(rr);

      rotated_y -= pitch / 4.0;

      if (std::abs(rotated_y) < 0.4)
        cell_color[y][x] = Color::RGB(255, 255, 0);
      else if (rotated_y < -0.4)
        cell_color[y][x] = Color::RGB(0, 120, 200);
      else
        cell_color[y][x] = Color::RGB(139, 90, 43);
    }
  }

  int cy = rows / 2;
  int cx = cols / 2;
  for (int i = -6; i <= -2; i++)
    if (cx + i >= 0)
      cell_color[cy][cx + i] = Color::RGB(255, 255, 0);
  for (int i = 2; i <= 6; i++)
    if (cx + i < cols)
      cell_color[cy][cx + i] = Color::RGB(255, 255, 0);
  cell_color[cy][cx] = Color::RGB(255, 255, 0);

  std::vector<Element> lines;
  for (int y = 0; y < rows; y++) {
    std::vector<Element> row;
    for (int x = 0; x < cols; x++)
      row.push_back(text(" ") | bgcolor(cell_color[y][x]));
    lines.push_back(hbox(row));
  }
  return vbox(lines) | border;
};

auto render_altimeter = [](double altitude) -> Element {
  Color alt_color = Color::Green;
  if (altitude < 100)
    alt_color = Color::Red;
  else if (altitude < 500)
    alt_color = Color::Yellow;

  std::string alt_str = std::to_string((int)altitude);
  while (alt_str.length() < 5)
    alt_str = " " + alt_str;

  return vbox({
      text("═══════════") | color(Color::GreenLight),
      text(" ALTIMETER ") | bold | center | color(Color::GreenLight),
      text("═══════════") | color(Color::GreenLight),
      separator(),
      text(alt_str) | bold | center | color(alt_color) | 
          size(WIDTH, EQUAL, 11),
      text("   FEET    ") | center | dim,
      separator(),
  }) | border;
};

auto render_airspeed = [](double speed) -> Element {
  Color spd_color = Color::Green;
  if (speed < 120)
    spd_color = Color::Red;
  else if (speed < 140 || speed > 280)
    spd_color = Color::Yellow;

  std::string spd_str = std::to_string((int)speed);
  while (spd_str.length() < 5)
    spd_str = " " + spd_str;

  return vbox({
      text("═══════════") | color(spd_color),
      text(" AIRSPEED  ") | bold | center | color(spd_color),
      text("═══════════") | color(spd_color),
      separator(),
      text(spd_str) | bold | center | color(spd_color) | 
          size(WIDTH, EQUAL, 11),
      text("   KNOTS   ") | center | dim,
      separator(),
  }) | border;
};

auto render_heading = [](double heading) -> Element {
  const int size = 15;
  const int cx = size / 2, cy = size / 2;
  std::vector<std::vector<std::string>> grid(
      size, std::vector<std::string>(size, " "));

  for (int y = 0; y < size; y++) {
    for (int x = 0; x < size; x++) {
      double dx = x - cx;
      double dy = y - cy;
      double dist = sqrt(dx * dx + dy * dy);
      if (dist > cy - 1.5 && dist < cy - 0.5)
        grid[y][x] = "·";
    }
  }

  double angle = heading;
  for (int r = 0; r < cy - 1; r++) {
    int nx = cx + (int)(sin(angle) * r);
    int ny = cy - (int)(cos(angle) * r);
    if (nx >= 0 && nx < size && ny >= 0 && ny < size)
      grid[ny][nx] = "│";
  }

  std::vector<Element> rows;
  for (auto& row : grid) {
    std::vector<Element> els;
    for (auto& c : row)
      els.push_back(text(c) | color(Color::Cyan));
    rows.push_back(hbox(els));
  }

  int hdg_deg = (int)(fmod((heading * 180.0 / M_PI) + 360.0, 360.0));
  
  std::string direction;
  if (hdg_deg < 23 || hdg_deg >= 338)
    direction = "N";
  else if (hdg_deg < 68)
    direction = "NE";
  else if (hdg_deg < 113)
    direction = "E";
  else if (hdg_deg < 158)
    direction = "SE";
  else if (hdg_deg < 203)
    direction = "S";
  else if (hdg_deg < 248)
    direction = "SW";
  else if (hdg_deg < 293)
    direction = "W";
  else
    direction = "NW";

  return vbox({
      text("═══════════════") | color(Color::Cyan),
      text("    HEADING    ") | bold | center | color(Color::Cyan),
      text("═══════════════") | color(Color::Cyan),
      vbox(rows),
      separator(),
      text(std::to_string(hdg_deg) + "° " + direction) | center | bold | color(Color::Cyan),
  }) | border;
};

auto render_vsi = [](const Plane& p) -> Element {
  double lift = std::max(0.0, (p.speed / 200.0) * (1.0 + p.pitch / 10.0));
  double sink = 50.0 - lift * 30.0 + std::abs(p.roll) * 0.5;
  double vertical_speed = (p.pitch * 3.0 - sink);
  double climb_rate = vertical_speed * 60.0;
  
  std::string vsi_display;
  Color vsi_color = Color::Green;
  
  if (climb_rate > 50) {
    vsi_display = "▲ +" + std::to_string((int)climb_rate);
    vsi_color = Color::Green;
  } else if (climb_rate < -50) {
    vsi_display = "▼ " + std::to_string((int)climb_rate);
    vsi_color = Color::Yellow;
  } else {
    vsi_display = "─  0";
    vsi_color = Color::GreenLight;
  }

  return vbox({
      text("═══════════") | color(Color::Yellow),
      text("VERT SPEED") | bold | center | color(Color::Yellow),
      text("═══════════") | color(Color::Yellow),
      separator(),
      text("           ") | center,
      text(vsi_display) | bold | center | color(vsi_color) | 
          size(WIDTH, EQUAL, 11),
      text("           ") | center,
      separator(),
      text("  FT/MIN   ") | center | dim,
  }) | border;
};

auto render_engine_panel = [](const Plane& p) -> Element {
  auto gauge_bar = [](double val, int width, Color c) {
    int filled = (int)(val * width);
    std::string bar(filled, '█');
    bar += std::string(width - filled, '░');
    return text(bar) | color(c);
  };

  Color thr_color = Color::RGB(0, 255, 150);
  if (p.throttle > 0.9)
    thr_color = Color::Red;
  else if (p.throttle < 0.3)
    thr_color = Color::Yellow;

  Color fuel_color = p.fuel < 0.2 ? Color::Red : Color::RGB(0, 200, 100);

  return vbox({
      text("════════════════════") | color(Color::White),
      text("   ENGINE THRUST    ") | bold | center | color(Color::White),
      text("════════════════════") | color(Color::White),
      separator(),
      text("Engine-1 Power:") | bold | color(Color::White),
      gauge_bar(p.throttle, 20, thr_color),
      text(std::to_string((int)(p.throttle * 100)) + " %") | center | bold | 
          color(thr_color),
      text("") | center,
      separator(),
      text("Fuel Remaining:") | bold | color(Color::White),
      gauge_bar(p.fuel, 20, fuel_color),
      text(std::to_string((int)(p.fuel * 100)) + " %") | center | bold | 
          color(fuel_color),
  }) | border;
};

auto render_warnings = [](const Plane& p) -> Element {
  std::vector<Element> warnings;

  if (p.altitude < 500)
    warnings.push_back(text("⚠ LOW ALTITUDE") | blink | color(Color::Red) | bold);
  if (p.speed < 120)
    warnings.push_back(text("⚠ STALL WARNING") | blink | color(Color::Red) | bold);
  if (p.fuel < 0.15)
    warnings.push_back(text("⚠ FUEL LOW") | blink | color(Color::Red) | bold);
  if (!p.gear_down && p.altitude < 1000)
    warnings.push_back(text("⚠ GEAR UP") | color(Color::Yellow) | bold);

  if (warnings.empty())
    warnings.push_back(text("ALL SYSTEMS NORMAL") | color(Color::Green));

  return vbox({
      text("═══ WARNINGS ═══") | bold | center | color(Color::White),
      separator(),
      vbox(warnings) | center,
  }) | border;
};

Element render_nav_display(const Plane& plane) {
  std::vector<Element> rows;

  double deg = fmod((plane.heading * 180.0 / M_PI) + 360.0, 360.0);

  std::string icon = "↑"; 
  if (deg < 22.5 || deg >= 337.5)
    icon = "↑"; 
  else if (deg < 67.5)
    icon = "↗";  
  else if (deg < 112.5)
    icon = "→"; 
  else if (deg < 157.5)
    icon = "↘";  
  else if (deg < 202.5)
    icon = "↓";  
  else if (deg < 247.5)
    icon = "↙"; 
  else if (deg < 292.5)
    icon = "←";  
  else
    icon = "↖";  

  for (int y = 0; y < world_map.size(); y++) {
    std::vector<Element> row;
    for (int x = 0; x < world_map[y].size(); x++) {
      if ((int)plane.map_y == y && (int)plane.map_x == x) {
        row.push_back(text(icon) | color(Color::Yellow) | bold);
      } else if (x >= plane.dest_x && x <= plane.dest_x + 1 &&
                 y >= plane.dest_y && y <= plane.dest_y + 1) {
        row.push_back(text("█") | color(Color::Magenta) | bold);
      } else {
        row.push_back(text("·") | color(Color::RGB(0, 100, 0)));
      }
    }
    rows.push_back(hbox(row));
  }

  return vbox({
      text("NAVIGATION DISPLAY") | bold | center | color(Color::Green),
      vbox(rows) | border,
      text("N↑ E→ S↓ W← | 1km/tile") | center | dim,
  });
};

int main() {
  auto screen = ScreenInteractive::Fullscreen();
  Plane plane;
  std::atomic<bool> running = true;

  Component gear_button =
      Button("GEAR", [&] { plane.gear_down = !plane.gear_down; });
  Component flaps_button =
      Button("FLAPS", [&] { plane.flaps = !plane.flaps; });
  auto buttons = Container::Horizontal({gear_button, flaps_button});

  auto cockpit_renderer = Renderer(buttons, [&] {
    std::string status = "FLIGHT";
    Color status_col = Color::Green;

    if (plane.landed) {
      status = "LANDED";
      status_col = Color::Green;
    } else if (plane.crashed) {
      status = "CRASHED";
      status_col = Color::Red;
    } else if (plane.fuel == 0) {
      status = "ENGINE FAILURE";
      status_col = Color::Red;
    }

    auto pfd_area = hbox({
        render_airspeed(plane.speed),
        separator(),
        render_artificial_horizon(plane.pitch, plane.roll),
        separator(),
        render_altimeter(plane.altitude),
    });

    auto secondary_instruments = hbox({
        render_heading(plane.heading),
        separator(),
        render_vsi(plane),
        separator(),
        render_nav_display(plane),
    });

    auto systems_panel = hbox({
        render_engine_panel(plane),
        separator(),
        render_warnings(plane),
    });

    auto landing_gear_panel = hbox({
        vbox({
            text("LANDING GEAR") | bold | center,
            text(plane.gear_down ? "▼ DOWN ▼" : "▲  UP  ▲") | center | bold |
                color(plane.gear_down ? Color::Green : Color::Red),
            gear_button->Render() | center,
        }) | border,
        separator(),
        vbox({
            text("FLAPS") | bold | center,
            text(plane.flaps ? "EXTENDED" : "RETRACTED") | center | bold |
                color(plane.flaps ? Color::Green : Color::GrayLight),
            flaps_button->Render() | center,
        }) | border,
        separator(),
        vbox({
            text("STATUS") | bold | center,
            text(status) | center | bold | color(status_col),
        }) | border | flex,
    });

    return vbox({
               text(" ╔═══════════════════════════════════════════════════════════╗") | color(Color::White)| center,
               text("║          TERMINAL FLIGHT SIMULATOR by pingminus           ║") | bold | color(Color::Cyan) | center,
               text(" ╚═══════════════════════════════════════════════════════════╝") | color(Color::White)| center,
               separator(),

               pfd_area | center,
               separator(),

               secondary_instruments | center,
               separator(),

               systems_panel | center,
               separator(),

               landing_gear_panel | center,
               separator(),

               text("─────────────────────────────────────────────────────────────") | dim,
               text("CONTROLS: ↑↓ Pitch | ←→ Roll | A/D Throttle | G Gear | F Flaps | Q Quit") |
                   center | color(Color::GrayLight),
               text("─────────────────────────────────────────────────────────────") | dim,
           }) | bgcolor(Color::RGB(10, 10, 15));
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
          if (plane.fuel > 0.0)
            plane.throttle = std::max(plane.throttle - 0.05, 0.0);
          break;

        case 'd':
          if (plane.fuel > 0.0)
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