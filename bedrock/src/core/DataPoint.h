#pragma once

#include <optional>
#include <string>

// Donnée exposée par un GameAdapter (voir shared/schemas/datapoint.schema.json) :
// valeur, disponibilité, provenance, horodatage, fraîcheur et motif.
namespace bcc {

enum class Availability { Available, Stale, Unavailable, Unsupported, PermissionRequired };
enum class DataSource { GameApi, RawInput, WindowManager, ServerQuery, SaveFile, Overlay, Simulation, User, None };

const char* availabilityId(Availability a); // identifiants du schéma (« available »…)
const char* dataSourceId(DataSource s);
const char* availabilityLabel(Availability a); // libellés français

template <class T>
struct DataPoint {
    std::optional<T> value;
    Availability availability = Availability::Unavailable;
    DataSource source = DataSource::None;
    double timestamp = 0; // secondes, horloge monotone
    double maxAge = 0;    // validité en secondes, 0 = illimitée
    std::string reason;

    static DataPoint available(T v, DataSource source, double timestamp, double maxAge = 0) {
        DataPoint d;
        d.value = std::move(v);
        d.availability = Availability::Available;
        d.source = source;
        d.timestamp = timestamp;
        d.maxAge = maxAge;
        return d;
    }
    static DataPoint missing(Availability a, std::string reason, DataSource source = DataSource::None, double timestamp = 0) {
        DataPoint d;
        d.availability = a;
        d.reason = std::move(reason);
        d.source = source;
        d.timestamp = timestamp;
        return d;
    }

    // Disponibilité réelle à l'instant `now` (une valeur trop ancienne devient « stale »).
    Availability availabilityAt(double now) const {
        if (availability == Availability::Available && maxAge > 0 && now - timestamp > maxAge) return Availability::Stale;
        return availability;
    }
    bool usable(double now) const { return value.has_value() && availabilityAt(now) == Availability::Available; }
};

} // namespace bcc
