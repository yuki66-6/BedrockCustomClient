#include "gui/MapPage.h"

#include "gui/Notifications.h"
#include "gui/TextureManager.h"
#include "gui/ThemeManager.h"
#include "gui/Widgets.h"
#include "map/WorldScanner.h"
#include "utils/Strings.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cmath>
#include <ctime>
#include <format>

namespace bcc {
namespace {

std::string formatDate(int64_t unix) {
    if (unix <= 0) return "—";
    std::time_t t = static_cast<std::time_t>(unix);
    std::tm local{};
    localtime_s(&local, &t);
    return std::format("{:02}/{:02}/{:04} {:02}:{:02}", local.tm_mday, local.tm_mon + 1, local.tm_year + 1900,
                       local.tm_hour, local.tm_min);
}

const Icon kWaypointIcons[] = {Icon::Pin, Icon::Home, Icon::Star, Icon::Flag, Icon::Skull};

} // namespace

MapPage::MapPage(TextureManager* textures, Notifications* notify)
    : textures_(textures), notify_(notify), sources_(map::createMapSources()) {
    map::registerDefaultMapLayers(layers_);
    for (auto& l : layers_) layerEnabled_[l->id()] = l->enabledByDefault();
    reloadExtensions();
}

MapPage::~MapPage() {
    if (job_.joinable()) job_.join();
    if (textures_) textures_->releasePrefix("map/");
}

void MapPage::reloadExtensions() {
    extensionErrors_ = extensions_.load(colors_);
}

void MapPage::refreshWorldList(bool gameRunning) {
    worlds_ = sources_[static_cast<size_t>(sourceIndex_)]->listWorlds();
    int target = worldChoice_ >= 0 ? worldChoice_
                                   : sources_[static_cast<size_t>(sourceIndex_)]->currentWorldIndex(worlds_, gameRunning);
    if (target >= static_cast<int>(worlds_.size())) target = -1;
    if (target != activeWorld_) {
        activeWorld_ = target;
        forceScan_ = true;
        keepViewOnNextResult_ = false;
    }
}

void MapPage::startScan() {
    if (jobRunning_ || activeWorld_ < 0 || activeWorld_ >= static_cast<int>(worlds_.size())) return;
    if (job_.joinable()) job_.join();
    map::WorldInfo world = worlds_[static_cast<size_t>(activeWorld_)];
    int dim = dimension_;
    map::BlockColors colors = colors_;
    map::MapSource* source = sources_[static_cast<size_t>(sourceIndex_)].get();
    bool local = dynamic_cast<map::BedrockSaveSource*>(source) != nullptr;
    jobRunning_ = true;
    jobProgress_ = 0.f;
    job_ = std::thread([this, world, dim, colors, source, local] {
        std::unique_ptr<map::WorldMapResult> r;
        if (local) {
            r = map::WorldScanner::scan(world, colors, dim, nullptr, [this](float p, const char* s) {
                jobProgress_ = p;
                std::lock_guard lock(jobMutex_);
                jobStage_ = s;
            });
        } else {
            r = source->buildMap(world, dim, colors);
        }
        std::lock_guard lock(jobMutex_);
        jobResult_ = std::move(r);
        jobRunning_ = false;
    });
}

void MapPage::finishScan() {
    std::unique_ptr<map::WorldMapResult> r;
    {
        std::lock_guard lock(jobMutex_);
        r = std::move(jobResult_);
    }
    if (job_.joinable()) job_.join();
    if (!r) return;
    result_ = std::move(r);
    uploadTextures();
    if (!keepViewOnNextResult_) centerOnDefault();
    keepViewOnNextResult_ = true;
}

void MapPage::uploadTextures() {
    if (textures_) textures_->releasePrefix("map/");
    tiles_.clear();
    if (!result_ || !textures_) return;
    for (auto& [key, pixels] : result_->regions) {
        std::string name = std::format("map/{}_{}", key.first, key.second);
        Texture t = textures_->createFromPixels(name, pixels.data(), map::kRegionPixels, map::kRegionPixels);
        if (t) tiles_[key] = t.id;
    }
    for (auto& [key, pixels] : result_->regions) std::vector<uint32_t>().swap(pixels); // libère la RAM
}

void MapPage::centerOnDefault() {
    if (!result_) return;
    if (result_->hasPlayer && result_->player.dimension == dimension_) {
        center_ = {result_->player.x, result_->player.z};
    } else if (result_->world.hasSpawn && dimension_ == 0) {
        center_ = {static_cast<float>(result_->world.spawnX), static_cast<float>(result_->world.spawnZ)};
    } else {
        center_ = {(result_->minBlockX + result_->maxBlockX) * 0.5f, (result_->minBlockZ + result_->maxBlockZ) * 0.5f};
    }
    targetZoom_ = zoom_ = 2.f;
}

void MapPage::draw(double now, bool gameRunning) {
    if (requestedWorld_ != -2) {
        worldChoice_ = requestedWorld_;
        requestedWorld_ = -2;
        lastWorldList_ = -100;
    }
    if (now - lastWorldList_ > 5.0) {
        lastWorldList_ = now;
        refreshWorldList(gameRunning);
    }
    if (!jobRunning_ && (jobResult_ || job_.joinable())) finishScan();
    bool autoDue = autoRefresh_ && gameRunning && worldChoice_ < 0 && now - lastScan_ > 15.0;
    if (!jobRunning_ && (forceScan_ || autoDue) && activeWorld_ >= 0) {
        forceScan_ = false;
        lastScan_ = now;
        startScan();
    }
    drawToolbar(gameRunning);
    drawCanvas(now);
    drawStatus();
}

void MapPage::drawToolbar(bool gameRunning) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    float sp = ImGui::GetStyle().ItemSpacing.x;

    // Source
    ImGui::SetNextItemWidth(150.f * sc);
    if (ImGui::BeginCombo("##source", sources_[static_cast<size_t>(sourceIndex_)]->name())) {
        for (int i = 0; i < static_cast<int>(sources_.size()); ++i)
            if (ImGui::Selectable(sources_[static_cast<size_t>(i)]->name(), i == sourceIndex_)) {
                sourceIndex_ = i;
                worldChoice_ = -1;
                activeWorld_ = -2;
                lastWorldList_ = -100;
                result_.reset();
                tiles_.clear();
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    // Monde
    std::string current = "Aucun monde";
    if (activeWorld_ >= 0 && activeWorld_ < static_cast<int>(worlds_.size())) current = worlds_[static_cast<size_t>(activeWorld_)].name;
    std::string preview = worldChoice_ < 0 ? "Monde actuel (auto) · " + current : current;
    ImGui::SetNextItemWidth(std::min(300.f * sc, ImGui::GetContentRegionAvail().x * 0.45f));
    if (ImGui::BeginCombo("##world", preview.c_str(), ImGuiComboFlags_HeightLarge)) {
        if (ImGui::Selectable("Monde actuel (auto)", worldChoice_ < 0)) {
            worldChoice_ = -1;
            lastWorldList_ = -100;
        }
        ui::tooltip("Choisit le monde dont la sauvegarde a été écrite le plus récemment (ta partie en cours).");
        ImGui::Separator();
        for (int i = 0; i < static_cast<int>(worlds_.size()); ++i) {
            auto& w = worlds_[static_cast<size_t>(i)];
            std::string label = std::format("{}   ·   {}   ·   {:.1f} Mo##w{}", w.name, formatDate(w.lastPlayed),
                                            static_cast<double>(w.sizeBytes) / (1024.0 * 1024.0), i);
            if (ImGui::Selectable(label.c_str(), worldChoice_ == i)) {
                worldChoice_ = i;
                lastWorldList_ = -100;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine(0, sp * 2.f);
    // Dimensions
    const char* dims[] = {"Overworld", "Nether", "End"};
    for (int d = 0; d < 3; ++d) {
        if (d) ImGui::SameLine();
        bool sel = dimension_ == d;
        if (sel) ImGui::PushStyleColor(ImGuiCol_Button, theme.accent(0.45f));
        if (ImGui::Button(dims[d]) && !sel) {
            dimension_ = d;
            forceScan_ = true;
            keepViewOnNextResult_ = false;
        }
        if (sel) ImGui::PopStyleColor();
    }

    // Deuxième ligne : actions
    if (ImGui::Button("Actualiser")) {
        forceScan_ = true;
        lastWorldList_ = -100;
    }
    ui::tooltip("Relit la sauvegarde du monde (lecture seule).");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Auto");
    ImGui::SameLine();
    if (ui::toggleSwitch("##auto", autoRefresh_)) autoRefresh_ = !autoRefresh_;
    ui::tooltip("Pendant que tu joues, relit la sauvegarde toutes les 15 s : la carte se dévoile au fil des sauvegardes du jeu.");
    ImGui::SameLine();
    if (ImGui::Button("Recentrer")) centerOnDefault();
    ImGui::SameLine();
    if (ImGui::Button("Calques")) ImGui::OpenPopup("##layers");
    if (ImGui::BeginPopup("##layers")) {
        for (auto& l : layers_) {
            bool& on = layerEnabled_[l->id()];
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(l->name());
            ui::sameLineRight(ImGui::GetFrameHeight() * 1.45f);
            ImGui::PushID(l->id());
            if (ui::toggleSwitch("##l", on)) on = !on;
            ImGui::PopID();
        }
        ImGui::Separator();
        if (ImGui::Button("Recharger les extensions")) {
            reloadExtensions();
            forceScan_ = true;
            keepViewOnNextResult_ = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Dossier")) {
            std::error_code ec;
            std::filesystem::create_directories(map::MapExtensions::folder(), ec);
            ShellExecuteW(nullptr, L"open", map::MapExtensions::folder().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        ImGui::TextDisabled("%zu extension(s) · %zu repère(s) · %zu zone(s)", extensions_.files().size(),
                            extensions_.markers().size(), extensions_.areas().size());
        for (auto& e : extensionErrors_) ImGui::TextColored({1.f, 0.6f, 0.4f, 1.f}, "%s", e.c_str());
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(fullscreen_ ? "Réduire" : "Plein écran")) fullscreen_ = !fullscreen_;
    if (gameRunning && worldChoice_ < 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("· suivi de la partie en cours");
    }
}

void MapPage::drawCanvas(double now) {
    ThemeManager& theme = ui::theme();
    float sc = theme.uiScale();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    float statusH = ImGui::GetFrameHeight() + 4.f * sc;
    ImVec2 size{avail.x, std::max(120.f, avail.y - statusH)};
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImVec2 p1{p0.x + size.x, p0.y + size.y};
    ImGui::InvisibleButton("##canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float r = theme.rounding() + 2.f;

    // Zoom fluide centré sur la souris, déplacement au glisser.
    map::MapDrawContext ctx;
    ctx.canvasMin = p0;
    ctx.canvasMax = p1;
    ctx.center = center_;
    ctx.zoom = zoom_;
    if (hovered && io.MouseWheel != 0.f) {
        ImVec2 before = ctx.screenToWorld(io.MousePos);
        targetZoom_ = std::clamp(targetZoom_ * std::pow(1.25f, io.MouseWheel), 0.06f, 24.f);
        lastMouseWorld_ = before;
    }
    if (zoom_ != targetZoom_) {
        float k = theme.animationsEnabled() ? 1.f - std::exp(-14.f * io.DeltaTime) : 1.f;
        ImVec2 anchor = hovered ? io.MousePos : ImVec2{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
        ImVec2 worldAtAnchor = ctx.screenToWorld(anchor);
        zoom_ += (targetZoom_ - zoom_) * k;
        if (std::fabs(zoom_ - targetZoom_) < 0.001f) zoom_ = targetZoom_;
        // Garde le point sous la souris immobile pendant le zoom.
        center_.x = worldAtAnchor.x - (anchor.x - (p0.x + p1.x) * 0.5f) / zoom_;
        center_.y = worldAtAnchor.y - (anchor.y - (p0.y + p1.y) * 0.5f) / zoom_;
    }
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.f)) {
        center_.x -= io.MouseDelta.x / zoom_;
        center_.y -= io.MouseDelta.y / zoom_;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
        center_ = ctx.screenToWorld(io.MousePos);
        targetZoom_ = std::min(24.f, targetZoom_ * 2.f);
    }
    ctx.center = center_;
    ctx.zoom = zoom_;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        popupWorld_ = ctx.screenToWorld(io.MousePos);
        waypointName_[0] = 0;
        ImGui::OpenPopup("##waypoint");
    }

    // Fond « brouillard » : zones jamais explorées.
    dl->PushClipRect(p0, p1, true);
    dl->AddRectFilled(p0, p1, IM_COL32(9, 13, 24, 235), r);
    float stepPx = 26.f * sc;
    float shift = static_cast<float>(std::fmod(now * 6.0, static_cast<double>(stepPx)));
    for (float x = p0.x - size.y + shift; x < p1.x; x += stepPx)
        dl->AddLine({x, p1.y}, {x + size.y, p0.y}, IM_COL32(255, 255, 255, 7));

    // Tuiles des régions découvertes.
    for (auto& [key, tex] : tiles_) {
        float bx = static_cast<float>(key.first * map::kRegionPixels), bz = static_cast<float>(key.second * map::kRegionPixels);
        ImVec2 a = ctx.worldToScreen(bx, bz), b = ctx.worldToScreen(bx + map::kRegionPixels, bz + map::kRegionPixels);
        if (b.x < p0.x || a.x > p1.x || b.y < p0.y || a.y > p1.y) continue;
        dl->AddImage(ImTextureRef(tex), a, b);
    }

    // Calques.
    ctx.drawList = dl;
    ctx.uiScale = sc;
    ctx.dimension = dimension_;
    ctx.world = result_.get();
    ctx.extensions = &extensions_;
    mouseInside_ = hovered;
    ctx.mouseInside = hovered;
    ctx.mouseWorld = lastMouseWorld_ = ctx.screenToWorld(io.MousePos);
    for (auto& l : layers_)
        if (layerEnabled_[l->id()]) l->draw(ctx);

    // Vignettage + bordure.
    dl->AddRect(p0, p1, theme.accentU32(0.35f * ImGui::GetStyle().Alpha), r, 1.2f);

    // Messages : chargement / vide / erreur.
    auto centerCard = [&](const std::string& title, const std::string& sub, float progress) {
        ImVec2 c{(p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f};
        float w = std::min(size.x - 40.f, 420.f * sc), h = (progress >= 0 ? 86.f : 70.f) * sc;
        ImVec2 a{c.x - w * 0.5f, c.y - h * 0.5f}, b{c.x + w * 0.5f, c.y + h * 0.5f};
        dl->AddRectFilled(a, b, IM_COL32(10, 16, 30, 235), r);
        dl->AddRect(a, b, theme.accentU32(0.5f), r);
        dl->AddText({a.x + 16.f * sc, a.y + 12.f * sc}, IM_COL32_WHITE, title.c_str());
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.86f, {a.x + 16.f * sc, a.y + 34.f * sc},
                    ui::colorU32(theme.textDim()), sub.c_str(), nullptr, w - 32.f * sc);
        if (progress >= 0) {
            ImVec2 bar0{a.x + 16.f * sc, b.y - 20.f * sc}, bar1{b.x - 16.f * sc, b.y - 14.f * sc};
            dl->AddRectFilled(bar0, bar1, IM_COL32(255, 255, 255, 25), 3.f);
            dl->AddRectFilled(bar0, {bar0.x + (bar1.x - bar0.x) * progress, bar1.y}, theme.accentU32(), 3.f);
        }
    };
    if (jobRunning_ && !result_) {
        std::string stage;
        {
            std::lock_guard lock(jobMutex_);
            stage = jobStage_;
        }
        centerCard("Lecture de la sauvegarde…", stage + " (lecture seule)", jobProgress_);
    } else if (activeWorld_ < 0) {
        centerCard("Aucun monde à afficher", sources_[static_cast<size_t>(sourceIndex_)]->emptyMessage(), -1.f);
    } else if (result_ && !result_->error.empty()) {
        centerCard("Rien à afficher ici", result_->error, -1.f);
    }
    if (jobRunning_ && result_) { // actualisation discrète
        dl->AddText({p0.x + 10.f * sc, p0.y + 8.f * sc}, theme.accentU32(0.9f), "Actualisation…");
    }
    dl->PopClipRect();
    drawWaypointPopup();
}

void MapPage::drawWaypointPopup() {
    if (!ImGui::BeginPopup("##waypoint")) return;
    int x = static_cast<int>(std::floor(popupWorld_.x)), z = static_cast<int>(std::floor(popupWorld_.y));
    ImGui::TextColored(ui::theme().accent(), "Bloc %d, %d", x, z);
    ImGui::TextDisabled("Chunk %d, %d", x >> 4, z >> 4);
    ImGui::Separator();
    ImGui::SetNextItemWidth(200.f * ui::theme().uiScale());
    bool enter = ImGui::InputTextWithHint("##wpname", "Nom du repère", waypointName_, sizeof(waypointName_),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
    const char* icons[] = {"Épingle", "Maison", "Étoile", "Drapeau", "Danger"};
    ImGui::SetNextItemWidth(200.f * ui::theme().uiScale());
    ImGui::Combo("##wpicon", &waypointIcon_, icons, 5);
    if ((ImGui::Button("Ajouter un repère ici") || enter) && waypointName_[0]) {
        std::string err;
        ImVec4 a = ui::theme().accent();
        uint32_t color = map::BlockColors::rgba(static_cast<uint8_t>(a.x * 255), static_cast<uint8_t>(a.y * 255),
                                                static_cast<uint8_t>(a.z * 255));
        if (extensions_.addWaypoint(waypointName_, x, z, dimension_, kWaypointIcons[waypointIcon_], color, &err)) {
            if (notify_) notify_->success(std::format("Repère « {} » ajouté", waypointName_));
        } else if (notify_) {
            notify_->error("Repère non enregistré : " + err);
        }
        ImGui::CloseCurrentPopup();
    }
    // Suppression d'un repère proche (créé depuis la carte).
    for (auto& m : extensions_.markers()) {
        if (m.source != map::MapExtensions::kWaypointsFile || m.dimension != dimension_) continue;
        if (std::abs(m.x - x) > 6 || std::abs(m.z - z) > 6) continue;
        if (ImGui::Button(std::format("Supprimer « {} »", m.label).c_str())) {
            std::string label = m.label, err;
            int mx = m.x, mz = m.z;
            if (!extensions_.removeWaypoint(label, mx, mz, &err) && notify_) notify_->error(err);
            ImGui::CloseCurrentPopup();
            break;
        }
    }
    ImGui::EndPopup();
}

void MapPage::drawStatus() {
    ImGui::AlignTextToFramePadding();
    int x = static_cast<int>(std::floor(lastMouseWorld_.x)), z = static_cast<int>(std::floor(lastMouseWorld_.y));
    if (mouseInside_) ImGui::Text("X %d   Z %d   ·   chunk %d, %d", x, z, x >> 4, z >> 4);
    else ImGui::TextDisabled("Glisser : déplacer · molette : zoom · double-clic : zoomer ici · clic droit : repère");
    if (result_) {
        std::string info = std::format("{} · {} · {} chunks découverts · lu en {:.2f} s", result_->world.name,
                                       map::dimensionName(result_->dimension), result_->chunks, result_->seconds);
        float w = ImGui::CalcTextSize(info.c_str()).x;
        ui::sameLineRight(w);
        ImGui::TextDisabled("%s", info.c_str());
        ui::tooltip("La carte montre la dernière sauvegarde du monde (Minecraft sauvegarde régulièrement pendant la partie). "
                    "Les zones sombres n'ont jamais été explorées.");
    }
}

} // namespace bcc
