#include "WhiteDwarfs.h"

/* Calculate eta_hydrogen from Claeys+ 2014, appendix B. We have changed the mass accretion limits for
 * Nomoto+ 2007 ones, after applying a quadratic fit to cover the low-mass end.
 *
 * double CalculateetaH(const double p_LogMassRate)
 *
 * @param   [IN]    p_LogMassRate        Logarithm of the mass transfer rate (Msun/yr)
 * @return                               eta_hydrogen, "hydrogen accretion efficiency"
 */

double WhiteDwarfs::CalculateetaH(const double p_LogMassRate) {
    double etaH = 0.0;
    // The following coefficients in massTransfer limits come from quadratic fits to Nomoto+ 2007 results (table 5) in Mass vs log10 Mdot space, to cover the low-mass end.
    double MdotCritH = MT_LIMIT_CRIT_NOMOTO_0 +  MT_LIMIT_CRIT_NOMOTO_1 * m_Mass +  MT_LIMIT_CRIT_NOMOTO_2 * m_Mass * m_Mass;
    double MdotLowH = MT_LIMIT_STABLE_NOMOTO_0 +  MT_LIMIT_STABLE_NOMOTO_1 * m_Mass +  MT_LIMIT_STABLE_NOMOTO_2 * m_Mass * m_Mass;
    if (utils::Compare(p_LogMassRate, MdotCritH) >= 0) {
        etaH = PPOW(10, MdotCritH - p_LogMassRate);
    } else if ((utils::Compare(p_LogMassRate, MdotCritH) < 0) && (utils::Compare(p_LogMassRate, MdotLowH) >= 0)) {
        etaH = 1.0;
    }
    return etaH;
}

/* Calculate eta_helium from Claeys+ 2014, appendix B. We have changed the mass accretion limits for
 * Piersanti+ 2014 ones. The different flashes regimes from Piersanti+ 2014 have been merged into one,
 * and the accumulation regime has been change so we can get double detonations. Finally, eta_KH04 has
 * also been updated with the accretion efficiency values from Piersanti+ 2014.
 *
 * double CalculateetaHe(const double p_LogMassRate)
 *
 * @param   [IN]    p_LogMassRate        Logarithm of the mass transfer rate (Msun/yr)
 * @return                               eta_hydrogen, "helium accretion efficiency"
 */

double WhiteDwarfs::CalculateetaHe(const double p_LogMassRate) {
    double etaHe = 0.0;
    // The following coefficients in massTransfer limits come from table A1 in Piersanti+ 2014.
    double MdotCritHe = MT_LIMIT_CRIT_PIERSANTI_0 + MT_LIMIT_CRIT_PIERSANTI_1 * m_Mass;
    double MdotLowHe = MT_LIMIT_STABLE_PIERSANTI_0 + MT_LIMIT_STABLE_PIERSANTI_1 * m_Mass;
    double MdotAccumulation = MT_LIMIT_DET_PIERSANTI_0 + MT_LIMIT_DET_PIERSANTI_1 * m_Mass;

    if (utils::Compare(p_LogMassRate, MdotCritHe) >= 0) {
        etaHe = PPOW(10, MdotCritHe - p_LogMassRate);
    } else if ((utils::Compare(p_LogMassRate, MdotCritHe) < 0) && (utils::Compare(p_LogMassRate, MdotLowHe) >= 0)) {
        etaHe = 1.0;
    } else if ((utils::Compare(p_LogMassRate, MdotLowHe) < 0) && (utils::Compare(p_LogMassRate, MdotAccumulation) >= 0)) {
        etaHe = CalculateetaPTY(p_LogMassRate);
    } else {
        etaHe = 1.0; // Modified so we can have double detonations
    }
    return etaHe;
}



/* Calculate accretion efficiency as indicated in Piersanti+ 2014. Their recipe works
 * for specific mass and Mdot values, so a better implementation requires interpolation and
 * extrapolation (specially towards the low-mass end). Right now, we just adopt a
 * piece-wise approach. Note that the authors also specify that this is based on the first
 * strong flash only, but we use it for all episodes.
 *
 * double CalculateetaPTY(const double p_LogMassRate)
 *
 * @param   [IN]    p_LogMassRate        log10 Mass transfer rate (Msun/yr)
 * @return                               etaPTY, accretion efficency during the first stron helium flash, Piersanti+ 2014
 */
double WhiteDwarfs::CalculateetaPTY(const double p_LogMassRate) {
    double etaPTY;
    double massRate = PPOW(10, p_LogMassRate); // The efficiency prescription uses plain mass rates, section A3 in Piersanti+ 2014.
    // Limits on each conditional statement come from masses from each model in Piersanti+ 2014. The final etaPTY value is based on table A3.
    if (utils::Compare(m_Mass, 0.6) <= 0) {
        etaPTY = 6e-3 + 5.1e-2*massRate + 8.3e-3*PPOW(massRate, 2) - 3.317e-4*PPOW(massRate,3);
    } else if ((m_Mass <= 0.7) && (m_Mass > 0.6)) {
        etaPTY = -3.5e-2 + 7.5e-2*massRate - 1.8e-3*PPOW(massRate, 2) + 3.266e-5*PPOW(massRate,3);
    } else if ((utils::Compare(m_Mass, 0.81) <= 0) && (utils::Compare(m_Mass, 0.7) > 0)) {
        etaPTY = 9.3e-2 + 1.8e-2*massRate + 1.6e-3*PPOW(massRate, 2) - 4.111e-5*PPOW(massRate,3);
    } else if ((utils::Compare(m_Mass, 0.92) <= 0) && (utils::Compare(m_Mass, 0.81) > 0)) {
        etaPTY = -7.59e-2 + 1.54e-2*massRate + 4e-4*PPOW(massRate, 2) - 5.905e-6*PPOW(massRate,3);
    } else {
        etaPTY = -0.323 + 4.1e-2*massRate - 7e-4*PPOW(massRate, 2) + 4.733e-6*PPOW(massRate,3);
    }
    return etaPTY;
}


/*
 * Calculate the luminosity of a White Dwarf as it cools
 *
 * Hurley et al. 2000, eq 90
 *
 *
 * double CalculateLuminosityOnPhase_Static(const double p_Mass, const double p_Time, const double p_Metallicity, const double p_BaryonNumber)
 *
 * @param   [IN]    p_Mass                      Mass in Msol
 * @param   [IN]    p_Time                      Time since White Dwarf formation in Myr
 * @param   [IN]    P_Metallicity               Metallicity of White Dwarf
 * @param   [IN]    p_BaryonNumber              Baryon number - differs per White Dwarf type (HeWD, COWD, ONeWD)
 * @return                                      Luminosity of a White Dwarf in Lsol
 */
double WhiteDwarfs::CalculateLuminosityOnPhase_Static(const double p_Mass, const double p_Time, const double p_Metallicity, const double p_BaryonNumber) {
    return (635.0 * p_Mass * PPOW(p_Metallicity, 0.4)) / PPOW(p_BaryonNumber * (p_Time + 0.1), 1.4);
}

/*
 * Calculate the radius of a white dwarf - good for all types of WD
 *
 * Hurley et al. 2000, eq 91 (from Tout et al. 1997)
 *
 *
 * double CalculateRadiusOnPhase_Static(const double p_Mass)
 *
 * @param   [IN]    p_Mass                      Mass in Msol
 * @return                                      Radius of a White Dwarf in Rsol (since WD is ~ Earth sized, expect answer around 0.009)
 */
double WhiteDwarfs::CalculateRadiusOnPhase_Static(const double p_Mass) {
    double MCH_Mass_one_third  = std::cbrt(MCH / p_Mass); 
    double MCH_Mass_two_thirds = MCH_Mass_one_third* MCH_Mass_one_third;
    return std::max(NEUTRON_STAR_RADIUS, 0.0115 * std::sqrt((MCH_Mass_two_thirds - 1.0/MCH_Mass_two_thirds )));
}

/* Increase shell size after mass transfer episode. Hydrogen and helium shells are kept separately.
 *
 * void ResolveShellChange(const double p_AccretedMass, bool p_HeRich) {
 *
 * @param   [IN]    p_AccretedMass              Mass accreted
 * @param   [IN]    p_HeRich                    Material is He-rich or not
 */
void WhiteDwarfs::ResolveShellChange(const double p_AccretedMass) {
    
    // RTW: Is this correct? Can we always assume the accretion regime is set correctly? What is the default case?
    // NRS: It looks ok to me. Though a future update might consider tracking hydrogen burning products (i.e which fraction of the accreted hydrogen turns into helium after a given time step).
    // NRS: as for the default case, are you talking about the shell? or the accretion regime? The shell being used is usually the hydrogen one, but the regime directly depends on the composition of the material being accreted and its rate, so it is not clear to me if using a default would be good.
    // RTW: I was more thinking about just the code default. I've added a warning below, and specified that no action should be taken. Let me know if you think there is a better alternative. 
    switch (m_AccretionRegime) {

        case ACCRETION_REGIME::HELIUM_ACCUMULATION:
        case ACCRETION_REGIME::HELIUM_FLASHES:
        case ACCRETION_REGIME::HELIUM_STABLE_BURNING:
        case ACCRETION_REGIME::HELIUM_OPT_THICK_WINDS:
        case ACCRETION_REGIME::HELIUM_WHITE_DWARF_HELIUM_SUB_CHANDRASEKHAR:
        case ACCRETION_REGIME::HELIUM_WHITE_DWARF_HELIUM_IGNITION:
	        m_HeShell += p_AccretedMass;
            break;

        case ACCRETION_REGIME::HYDROGEN_FLASHES:
        case ACCRETION_REGIME::HYDROGEN_STABLE_BURNING:
        case ACCRETION_REGIME::HYDROGEN_OPT_THICK_WINDS:
        case ACCRETION_REGIME::HELIUM_WHITE_DWARF_HYDROGEN_FLASHES:
        case ACCRETION_REGIME::HELIUM_WHITE_DWARF_HYDROGEN_ACCUMULATION:
	        m_HShell += p_AccretedMass;
            break;

        default:
            SHOW_WARN(ERROR::WARNING, "Accretion Regime not set for WD, no mass added to shell.");                // show warning 
    }
}

