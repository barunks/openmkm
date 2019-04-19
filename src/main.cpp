#include <string>
#include <vector>
#include <iostream>
#include <fstream>
/*
#include <memory>
#include <map>

include <yaml-cpp/yaml.h>
#include "cantera/base/ct_defs.h"
#include "cantera/thermo/SurfLatIntPhase.h"
#include "cantera/thermo/SurfPhase.h"
#include "cantera/thermo/LateralInteraction.h"
#include "cantera/thermo/ThermoPhase.h"
#include "cantera/zeroD/Reactor.h"
#include "cantera/zeroD/ReactorNet.h"
#include "cantera/zeroD/Reservoir.h"
#include "cantera/zeroD/ReactorSurface.h"
#include "cantera/zeroD/flowControllers.h"
*/

#include "cantera/base/stringUtils.h"
#include "cantera/IdealGasMix.h"
#include "cantera/thermo/StoichSubstance.h"
#include "cantera/InterfaceLatInt.h"

#include "io.h"
#include "reactor.h"

using namespace std;
using namespace Cantera;
using namespace HeteroCt;



std::map<std::string, RctrType> RctrTypeMap = {{"batch", BATCH}, 
                                               {"cstr", CSTR}, 
                                               {"pfr_0d", PFR_0D}, 
                                               {"pfr", PFR}}; 


int main(int argc, char* argv[]) 
{
    ofstream gen_info ("general_info.out", ios::out);
    print_htrct_header(gen_info);
    if (argc < 3) {
        // TODO: Throw error
        ;
    };

    string tube_file_name {argv[1]};       // Tube drive file in YAML format
    string phase_file_name {argv[2]};      // Thermodata in either CTI/XML formats
    auto tube_node = YAML::LoadFile(tube_file_name);
    
    // Read the phase definitions
    auto phase_node = tube_node["phases"];
    string gas_phase_name = phase_node["gas"]["name"].as<string>();
    string gas_phase_X = phase_node["gas"]["initial_state"].as<string>();
    string blk_phase_name = phase_node["bulk"]["name"].as<string>();
    auto surface_nodes = phase_node["surfaces"];
    vector<string> surface_phase_names; 
    vector<string> surface_states;
    for (size_t i=0;  i < surface_nodes.size(); i++) {
        surface_phase_names.push_back(surface_nodes[i]["name"].as<string>());
        surface_states.push_back(surface_nodes[i]["initial_state"].as<string>());
    }
    for (const auto& surf_name: surface_phase_names) 
        cout << surf_name << endl;


    auto gas = make_shared<IdealGasMix>(phase_file_name, gas_phase_name);
    auto bulk = make_shared<StoichSubstance>(phase_file_name, blk_phase_name);
    vector<ThermoPhase*> gb_phases {gas.get(), bulk.get()};
    vector<shared_ptr<InterfaceInteractions>> surf_phases;
    vector<Kinetics*> all_km {gas.get()};
    for (size_t i=0; i < surface_phase_names.size(); i++) {
        auto surf_ph_name = surface_phase_names[i];
        auto surf = make_shared<InterfaceInteractions>(phase_file_name, 
                                                       surf_ph_name, 
                                                       gb_phases);
        surf_phases.push_back(surf);
        all_km.push_back(surf.get());
    }

    /* Read the reactor definition */
    auto rctr_node = tube_node["reactor"];
    if (!rctr_node) {
        // TODO: Throw error
        ;
    };

    // Set the temp and press for all phases
    auto temp = strSItoDbl(rctr_node["temperature"].as<string>());
    auto press = strSItoDbl(rctr_node["pressure"].as<string>());
    gas->setState_TPX(temp, press, gas_phase_X);
    bulk->setState_TP(temp, press);
    for (size_t i=0; i < surface_phase_names.size(); i++) {
        surf_phases[i]->setState_TP(temp, press);
        surf_phases[i]->setCoveragesByName(surface_states[i]);
    }

    /* Print the reaction thermodynamic info */
    size_t n_rxns = 0;
    for (const auto km : all_km) {
        n_rxns += km->nReactions();
    }
    cout << "Total # of reactions: " << n_rxns << endl;

    print_rxn_enthalpy(all_km, gas->temperature(), "Hrxn.out");
    print_rxn_entropy(all_km, "Srxn.out");
    print_rxn_gibbs(all_km, gas->temperature(), "Grxn.out");
    print_rxn_kf(all_km,  "kf.out");
    print_rxn_kc(all_km,  "kc.out");
    print_rxn_kr(all_km,  "kr.out");


    auto rctr_type_node = rctr_node["type"];
    auto rctr_type = RctrTypeMap[rctr_type_node.as<string>()];

    if (rctr_type == BATCH || rctr_type == CSTR || rctr_type == PFR_0D) { // 0d reactors
        run_0d_reactor(tube_node, gas, surf_phases, gen_info);

    }
    else if (rctr_type == PFR) { // 1d reactor
        ; //TODO: Add 1d PFR implementation
    }
    
}
