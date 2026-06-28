#ifndef ARIA_VENDOR_IVSTUNITS_H
#define ARIA_VENDOR_IVSTUNITS_H

#if defined(ARIA_FEATURE_VST3)

#include "../base/funknown.h"

namespace Steinberg {
namespace Vst {

struct UnitInfo {
    int32_t id;
    TChar name[64];
    int32_t parent_unit_id;
    int32_t program_list_id;
};

class IUnitInfo : public FUnknown {
public:
    static const FUID iid;

    virtual int32_t getUnitCount() = 0;
    virtual TResult getUnitInfo(int32_t unit_index, UnitInfo& info) = 0;
    virtual int32_t getProgramListCount() = 0;
    virtual TResult getProgramListInfo(int32_t list_index, void* info) = 0;
    virtual TResult getProgramName(int32_t list_id, int32_t program_index, TChar* name) = 0;
    virtual TResult getProgramInfo(int32_t list_id, int32_t program_index, void* attribute_id, TChar* attribute_value) = 0;
    virtual TResult hasProgramPitchNames(int32_t list_id, int32_t program_index) = 0;
    virtual TResult getProgramPitchName(int32_t list_id, int32_t program_index, int16_t pitch, TChar* name) = 0;
    virtual int32_t getSelectedUnit() = 0;
    virtual TResult selectUnit(int32_t unit_id) = 0;
    virtual TResult getUnitByBus(MediaTypes type, BusDirection dir, int32_t bus_index, int32_t channel, int32_t& unit_id) = 0;
    virtual TResult setUnitProgramData(int32_t list_or_program_id, int32_t unit_id, FUnknown* data) = 0;
};

} // namespace Vst
} // namespace Steinberg

#endif // ARIA_FEATURE_VST3

#endif // ARIA_VENDOR_IVSTUNITS_H