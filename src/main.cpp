//
//  COMPAS main
//
#include <ctime>
#include <chrono>
#include <string>
#include <sstream>
#include <fstream>
#include <tuple>
#include <vector>

#include "constants.h"
#include "typedefs.h"

#include "utils.h"
#include "Options.h"
#include "Rand.h"
#include "Log.h"

#include "AIS.h"
#include "Star.h"
#include "BinaryStar.h"

OBJECT_ID globalObjectId = 1;                                   // used to uniquely identify objects - used primarily for error printing
OBJECT_ID m_ObjectId     = 0;                                   // object id for main - always 0


class AIS;
class Star;
class BinaryStar;

OBJECT_ID    ObjectId()    { return m_ObjectId; }
OBJECT_TYPE  ObjectType()  { return OBJECT_TYPE::MAIN; }
STELLAR_TYPE StellarType() { return STELLAR_TYPE::NONE; }


/*
 * Open the SSE grid file, read and parse the header record
 *
 *
 * std::tuple<int, std::vector<std::string>, std::string> OpenSSEGridFile(std::ifstream &p_Grid, const std::string p_Filename)
 *
 * @param   [IN/OUT]    p_Grid                  The std::ifstream  to which the file should be opened
 * @param   [IN]        p_Filename              The filename of the Grid file
 * @return                                      tuple containing the line number of the next line to be read and a vector of header strings
 */
std::tuple<int, std::vector<std::string>> OpenSSEGridFile(std::ifstream &p_Grid, const std::string p_Filename) {

    std::vector<std::string> gridHeaders = {};                                                                  // grid file headers

    int mass        = 0;                                                                                        // count 'Mass" occurrences
    int metallicity = 0;                                                                                        // count 'Metallicity" occurrences
    int unknown     = 0;                                                                                        // count unknown occurrences

    int tokenCount = 0;                                                                                         // token count - to check if header should be present
    int currentPos = p_Grid.tellg();                                                                            // current file position - to rewind if no header

    int lineNo = 1;                                                                                             // line number - for error messages

    if (!OPTIONS->GridFilename().empty()) {                                                                     // have grid filename?

        p_Grid.open(OPTIONS->GridFilename());                                                                   // yes - open the file
        if (p_Grid.fail()) {                                                                                    // open ok?
            SAY(ERR_MSG(ERROR::FILE_OPEN_ERROR) << " " << OPTIONS->GridFilename());                             // no - show error
        }
        else {                                                                                                  // file open ok

            bool readFailed  = false;                                                                           // record read ok?
            bool emptyRecord = true;                                                                            // record empty?
            std::string record;
            while (emptyRecord && !readFailed) {                                                                // get the header - first non-empty record

                currentPos = p_Grid.tellg();                                                                    // current file position
                if (std::getline(p_Grid, record)) {                                                             // read next record
                    readFailed = false;                                                                         // ok

                    size_t hashPos = record.find("#");                                                          // find first occurrence of "#"
                    if (hashPos != std::string::npos) record.erase(hashPos, record.size() - hashPos);           // if "#" found, prune it and everything after it (ignore comments)

                    while (record.size() > 0 && record[0] == ' ') record.erase(0, 1);                           // strip any leading ' ' characters from the record

                    while (record.size() > 0               &&
                          (record[record.size()-1] == '\r' ||
                           record[record.size()-1] == '\n' ||
                           record[record.size()-1] == ' '  )) record.erase(record.size()-1, 1);                 // strip any trailing '\r', '\n' and ' ' characters from the record

                    if (record.empty()) {
                        lineNo++;                                                                               // increment line number
                        continue;                                                                               // skip empty record
                    }

                    emptyRecord = false;                                                                        // have non-empty record

                    record = utils::ToUpper(record);                                                            // upshift

                    std::stringstream recordSS(record);                                                         // open stream on record
                    std::string token;
                    while (std::getline(recordSS, token, ',')) {                                                // get token from record read

                        tokenCount++;                                                                           // increment token count - even if it's empty, it's a token

                        while (token.size() > 0 && token[0] == ' ') token.erase(0, 1);                          // strip any leading ' ' characters from the token

                        while (token.size() > 0              &&
                              (token[token.size()-1] == '\r' ||
                               token[token.size()-1] == '\n' ||
                               token[token.size()-1] == ' ')) token.erase(token.size()-1, 1);                   // strip any trailing '\r', '\n' and ' ' characters from the token

                        if (token.empty()) {                                                                    // empty token?
                            SAY(ERR_MSG(ERROR::GRID_FILE_EMPTY_HEADER));                                        // show error
                            continue;                                                                           // next token
                        }

                        switch (_(token.c_str())) {                                                             // which column header?

                            case _("MASS")       : mass++;        gridHeaders.push_back(token); break;          // Mass

                            case _("METALLICITY"): metallicity++; gridHeaders.push_back(token); break;          // Metallicity

                            default              : unknown++;                                                   // unknown - deal with this later
                        }
                    }

                    lineNo++;                                                                                   // increment line number
                }
                else readFailed = true;                                                                         // read failed - may be EOF
            }
        }
    }

    if (mass != 1 || metallicity > 1) {                                                                         // check we have all the headers we need, and in the right numbers
                                                                                                                // we don't, but maybe this wasn't a header record
        if (tokenCount > 1 || mass >= 1 || metallicity >= 1) {                                                  // more than 1 column, or we got some header strings, so should have been a header

            if (mass < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Mass")                              // no 'Mass'
            else if (mass > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Mass");                      // duplicate 'Mass'

            if (metallicity < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Metallicity")                // no 'Metallicity'
            else if (metallicity > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Metallicity");        // duplicate 'Metallicity'

            if (unknown > 0) SAY(ERR_MSG(ERROR::GRID_FILE_UNKNOWN_HEADER));                                     // unknown header string

            if (p_Grid.is_open()) p_Grid.close();                                                               // must have been a problem - close the grid file
        }
        else {                                                                                                  // otherwise leave the file open and ...
            gridHeaders = { "MASS" };                                                                           // ... assume we have a header for "Mass" and ...
            p_Grid.seekg(currentPos);                                                                           // ... rewind to the record just read
        }
    }

    return std::make_tuple(lineNo, gridHeaders);
}


/*
 * Read and parse the next record from the SSE grid file
 *
 * Returns the following data values from the Grid file in the following order:
 *
 *    <Mass, Metallicity>
 *
 * Missing values are treated as zero (0.0) - a warning will be issued, and reading of the Grid file continues
 * (A value is considered missing only if there is a header for the column, but no data value in the column)
 * Invalid values are treated as errors - an error will be issued and reading of the Grid file stopped
 * Negative values are treated as errors - an error will be issued and reading of the Grid file stopped
 *
 * If the SSE Grid file contains Mass values only, Metallicity will be set to the value returned by OPTIONS->Metallicity()
 *
 * std::tuple<int, std::vector<double>> ReadSSEGridRecord(std::ifstream &p_Grid, const std::vector<std::string> p_GridHeaders, const int p_LineNo)
 *
 * @param   [IN/OUT]    p_Grid                  The Grid file (std::ifstream)
 * @param   [IN]        p_GridHeaders           Vector of header strings returned by OpenSSEGridFile()
 * @param   [IN]        p_LineNo                The line number of the next line to be read
 * @return                                      tuple containing the line number of the next line to be read, and a vector of data values
 */
std::tuple<int, std::vector<double>> ReadSSEGridRecord(std::ifstream &p_Grid, const std::vector<std::string> p_GridHeaders, const int p_LineNo) {

    bool error = false;

    std::vector<double> gridValues = {0.0, 0.0};                                                                // grid record values

    int lineNo = p_LineNo;                                                                                      // line number - for error messages

    bool emptyRecord = true;
    std::string record;
    while (!error && emptyRecord && std::getline(p_Grid, record)) {                                             // read the next record of the file

        size_t hashPos = record.find("#");                                                                      // find first occurrence of "#"
        if (hashPos != std::string::npos) record.erase(hashPos, record.size() - hashPos);                       // if "#" found, prune it and everything after it (ignore comments)

        for (size_t pos = 0; pos < record.size(); pos++) if (record[pos] == '\t') record[pos] = ' ';            // replace tab with space

        while (record.size() > 0 && record[0] == ' ') record.erase(0, 1);                                       // strip any leading ' ' characters from the record

        while (record.size() > 0               &&
              (record[record.size()-1] == '\r' ||
               record[record.size()-1] == '\n' ||
               record[record.size()-1] == ' '  )) record.erase(record.size()-1, 1);                             // strip any trailing '\r', '\n' and ' ' characters from the record

        if (record.empty()) {
            lineNo++;                                                                                           // increment line number
            continue;                                                                                           // skip empty record
        }
        emptyRecord = false;                                                                                    // have non-empty record

        std::istringstream recordSS(record);                                                                    // open input stream on record

        size_t column = 0;                                                                                      // start at column 0
        std::string token;                                                                                      // token from record
        while (!error && std::getline(recordSS, token, ',')) {                                                  // get next token from record read

            while (token.size() > 0 && token[0] == ' ') token.erase(0, 1);                                      // strip any leading ' ' characters from the token

            while (token.size() > 0              &&
                  (token[token.size()-1] == '\r' ||
                   token[token.size()-1] == '\n' ||
                   token[token.size()-1] == ' '  )) token.erase(token.size()-1, 1);                             // strip any trailing '\r', '\n' and ' ' characters from the token

            if (column >= p_GridHeaders.size()) {
                SAY(ERR_MSG(ERROR::GRID_FILE_EXTRA_COLUMN) << " ignored");                                      // show error
            }
            else {

                if (token.empty()) {                                                                            // empty token?
                    if (utils::Equals(p_GridHeaders[column], "METALLICITY")) {                                  // yes metallicity column?
                        token = std::to_string(OPTIONS->Metallicity());                                         // yes - use default value from program option
                        SAY(ERR_MSG(ERROR::GRID_FILE_DEFAULT_METALLICITY) << " at line " << lineNo);            // let the user know we're using default metallicity
                    }
                    else {                                                                                      // mass
                        token = "0.0";                                                                          // use 0.0
                        SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_DATA) << " at line " << lineNo << ": 0.0 used");   // show warning
                    }
                }

                double value;
                std::istringstream tokenSS(token);                                                              // open input stream on token
                tokenSS >> value;                                                                               // read double from token
                if (tokenSS.fail()) {                                                                           // check for valid conversion
                    error = true;                                                                               // set error flag
                    SAY(ERR_MSG(ERROR::GRID_FILE_INVALID_DATA) << "at line " << lineNo << ": " << token);       // show error
                }
                else if (value < 0.0) {                                                                         // value < 0?
                    error = true;                                                                               // set error flag
                    SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token);     // show error
                }

                if (!error) {
                    std::string columnName = p_GridHeaders[column];                                             // get column name for value
                    switch (_(columnName.c_str())) {                                                            // which column?

                        case _("MASS")       : gridValues[0] = value; break;                                    // Mass

                        case _("METALLICITY"): gridValues[1] = value; break;                                    // Metallicity

                        default:                                                                                // unknown - this shouldn't happen
                            SAY(ERR_MSG(ERROR::GRID_FILE_UNKNOWN_HEADER) << " " << columnName);                 // show error
                    }
                }
            }
            column++;                                                                                           // next column
        }

        if (!error) {
            if (column < p_GridHeaders.size()) {
                for (size_t c = column; c < p_GridHeaders.size(); c++) {
                    if (utils::Equals(p_GridHeaders[column], "METALLICITY")) {                                  // metallicity column?
                        SAY(ERR_MSG(ERROR::GRID_FILE_DEFAULT_METALLICITY) << " at line " << lineNo);            // let the user know we're using default metallicity
                        gridValues[1] = OPTIONS->Metallicity();                                                 // use program option for metallicity
                    }
                    else {                                                                                      // mass
                        SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_DATA) << " at line " << lineNo << ": 0.0 used");   // show warning
                    }
                }
            }

            if (p_GridHeaders.size() == 1 && utils::Equals(p_GridHeaders[0], "Mass")) {                         // grid file has only Mass values?
                SAY(ERR_MSG(ERROR::GRID_FILE_DEFAULT_METALLICITY));                                             // let the user know we're using default metallicity
                gridValues[1] = OPTIONS->Metallicity();                                                         // yes - use program option for metallicity
            }
        }
        lineNo++;                                                                                               // increment line number
    }

    if (error) gridValues = {};                                                                                 // return empty vector if error occurred

    return std::make_tuple(lineNo, gridValues);
}


/*
 * Evolve single stars
 *
 *
 * void EvolveSingleStars()
 */
void EvolveSingleStars() {

    EVOLUTION_STATUS evolutionStatus = EVOLUTION_STATUS::CONTINUE;

    auto wallStart = std::chrono::system_clock::now();                                                                          // start wall timer
    clock_t clockStart = clock();                                                                                               // start CPU timer

    if (!OPTIONS->Quiet()) {
        std::time_t timeStart = std::chrono::system_clock::to_time_t(wallStart);
        SAY("Start generating stars at " << std::ctime(&timeStart));
    }

    std::ifstream grid;                                                                                                         // grid file
    std::vector<std::string> gridHeaders;                                                                                       // grid file headers

    int nStars;                                                                                                                 // the number of stars to evolve

    double massIncrement = (OPTIONS->SingleStarMassMax() - OPTIONS->SingleStarMassMin()) / OPTIONS->SingleStarMassSteps();      // use user-specified values if no grid file

    int lineNo;                                                                                                                 // grid file line number - for error messages
    if (!OPTIONS->GridFilename().empty()) {                                                                                     // have grid filename?
        std::tie(lineNo, gridHeaders) = OpenSSEGridFile(grid, OPTIONS->GridFilename());                                         // yes - read grid file headers
        if (!grid.is_open()) evolutionStatus = EVOLUTION_STATUS::ERROR;                                                         // failed to open/read grid file
        nStars = 1;                                                                                                             // ... for each data record in the grid file
    }
    else {
        nStars = OPTIONS->SingleStarMassSteps();                                                                                // user specified
    }


    // Loop over stars to evolve

    evolutionStatus = EVOLUTION_STATUS::CONTINUE;

    int index  = 0;
    Star* star = nullptr;
    while (evolutionStatus == EVOLUTION_STATUS::CONTINUE && index < nStars) {

        // single stars are provided with a random seed
        // when they are constituent stars of a binary the binary provides the seed
        // here we generate the seed for the single star

        unsigned long int randomSeed = 0l;
        if (OPTIONS->FixedRandomSeed()) {                                                                                       // user supplied seed for the random number generator?
            randomSeed = RAND->Seed(OPTIONS->RandomSeed() + index);                                                             // yes - this allows the user to reproduce results for each binary
        }
        else {                                                                                                                  // no
            randomSeed = RAND->Seed(RAND->DefaultSeed() + index);                                                               // use default seed (based on system time) + binary id
        }

        // create the star

        double initialMass = 0.0;
        if (!OPTIONS->GridFilename().empty()) {                                                                                 // have grid filename?
            if (grid.is_open()) {                                                                                               // yes - grid file open?

                std::vector<double> gV;                                                                                         // yes - read the next record of grid values
                std::tie(lineNo, gV) = ReadSSEGridRecord(grid, gridHeaders, lineNo);                                            // read grid file record
                if (grid.fail() || gV.empty()) {                                                                                // read failed?
                    grid.close();                                                                                               // yes - EOF or error - stop reading, close file
                    if (gV.empty()) evolutionStatus = EVOLUTION_STATUS::STOPPED;                                                // if error, set error
                    else            evolutionStatus = EVOLUTION_STATUS::DONE;                                                   // otherwise, set done
                }
                else {                                                                                                          // no
                    initialMass = gV[0];                                                                                        // for status message
                    nStars++;                                                                                                   // not done yet...
                    delete star;
                    star = new Star(randomSeed, gV[0], gV[1]);                                                                  // create a star with required mass and metallicity, and...
                }
            }
            else evolutionStatus = EVOLUTION_STATUS::STOPPED;                                                                   // must have been a problem
        }
        else {                                                                                                                  // no grid file - use user-specified values
            initialMass = OPTIONS->SingleStarMassMin() + (index * massIncrement);                                               // for status message
            delete star;
            star = new Star(randomSeed, initialMass, OPTIONS->Metallicity());                                                   // create a star with required mass and metallicity, and...
        }

        if (evolutionStatus == EVOLUTION_STATUS::CONTINUE) {                                                                    // still good?

            star->Evolve(index);                                                                                                // yes  - evolve the star

            if (!OPTIONS->Quiet()) {                                                                                            // announce result of evolving the star
                SAY(index               <<
                    ": RandomSeed = "   <<
                    randomSeed          <<
                    ", Initial Mass = " <<
                    initialMass         <<
                    ", Metallicity = "  <<
                    star->Metallicity() <<
                    ", "                <<
                    STELLAR_TYPE_LABEL.at(star->StellarType()));
            }
        }

        if (!LOGGING->CloseStandardFile(LOGFILE::SSE_PARAMETERS)) {                                                             // close single star output file
            SHOW_WARN(ERROR::FILE_NOT_CLOSED);                                                                                  // close failed - show warning
            evolutionStatus = EVOLUTION_STATUS::STOPPED;                                                                        // this will cause problems later - stop evolution
        }

        ERRORS->Clean();                                                                                                        // clean the dynamic error catalog

        index++;                                                                                                                // next...
    }
    delete star;

    if (evolutionStatus == EVOLUTION_STATUS::CONTINUE && index >= nStars) evolutionStatus = EVOLUTION_STATUS::DONE;             // set done


    // announce result
    if (!OPTIONS->Quiet()) {
        if (evolutionStatus != EVOLUTION_STATUS::CONTINUE) {
            SAY("\n" << EVOLUTION_STATUS_LABEL.at(evolutionStatus));
        }
    }

    if (!OPTIONS->Quiet()) {

        double cpuSeconds = (clock() - clockStart) / (double) CLOCKS_PER_SEC;                                                   // stop CPU timer and calculate seconds

        auto wallEnd = std::chrono::system_clock::now();                                                                        // stope wall timer
        std::time_t timeEnd = std::chrono::system_clock::to_time_t(wallEnd);                                                    // get end time and date

        SAY("\nEnd generating stars at " << std::ctime(&timeEnd));
        SAY("Clock time = " << cpuSeconds << " CPU seconds");


        std::chrono::duration<double> wallSeconds = wallEnd - wallStart;                                                        // elapsed seconds

        int wallHH = (int)(wallSeconds.count() / 3600.0);                                                                       // hours
        int wallMM = (int)((wallSeconds.count() - ((double)wallHH * 3600.0)) / 60.0);                                           // minutes
        int wallSS = (int)(wallSeconds.count() - ((double)wallHH * 3600.0) - ((double)wallMM * 60.0));                          // seconds

        SAY("Wall time  = " << wallHH << ":" << wallMM << ":" << wallSS << " (hh:mm:ss)");
    }
}


/*
 * Open the BSE grid file, read and parse the header record
 *
 *
 * std::tuple<int, std::vector<std::string>> OpenBSEGridFile(std::ifstream &p_Grid, const std::string p_Filename)
 *
 * @param   [IN/OUT]    p_Grid                  The std::ifstream  to which the file should be opened
 * @param   [IN]        p_Filename              The filename of the Grid file
 * @return                                      tuple containing the line number of the next line to be read, and a vector of header strings
 */
std::tuple<int, std::vector<std::string>> OpenBSEGridFile(std::ifstream &p_Grid, const std::string p_Filename) {

    std::vector<std::string> gridHeaders = {};                                                                                  // grid file headers

    int mass1            = 0;                                                                                                   // count 'Mass_1" occurrences
    int mass2            = 0;                                                                                                   // count 'Mass_2" occurrences
    int metallicity1     = 0;                                                                                                   // count 'Metallicity_1" occurrences
    int metallicity2     = 0;                                                                                                   // count 'Metallicity_2" occurrences
    int separation       = 0;                                                                                                   // count 'Separation" occurrences
    int eccentricity     = 0;                                                                                                   // count 'Eccentricity" occurrences
    int period           = 0;                                                                                                   // count 'Period" occurrences

    int kickVelocity1    = 0;                                                                                                   // count 'Kick_Velocity_1" occurrences
    int kickTheta1       = 0;                                                                                                   // count 'Kick_Theta_1" occurrences
    int kickPhi1         = 0;                                                                                                   // count 'Kick_Phi_1" occurrences
    int kickMeanAnomaly1 = 0;                                                                                                   // count 'Kick_Mean_Anomaly_1" occurrences
    int kickVelocity2    = 0;                                                                                                   // count 'Kick_Velocity_2" occurrences
    int kickTheta2       = 0;                                                                                                   // count 'Kick_Theta_2" occurrences
    int kickPhi2         = 0;                                                                                                   // count 'Kick_Phi_2" occurrences
    int kickMeanAnomaly2 = 0;                                                                                                   // count 'Kick_Mean_Anomaly_2" occurrences

    int lineNo = 1;                                                                                                             // line number - for error messages

    if (!OPTIONS->GridFilename().empty()) {                                                                                     // have grid filename?

        p_Grid.open(OPTIONS->GridFilename());                                                                                   // yes - open the file
        if (p_Grid.fail()) {                                                                                                    // open ok?
            SAY(ERR_MSG(ERROR::FILE_OPEN_ERROR) << " " << OPTIONS->GridFilename());                                             // no - show error
        }
        else {                                                                                                                  // file open ok

            bool emptyRecord = true;
            std::string record;
            while (emptyRecord && std::getline(p_Grid, record)) {                                                               // read the header - first non-empty record

                size_t hashPos = record.find("#");                                                                              // find first occurrence of "#"
                if (hashPos != std::string::npos) record.erase(hashPos, record.size() - hashPos);                               // if "#" found, prune it and everything after it (ignore comments)

                while (record.size() > 0 && record[0] == ' ') record.erase(0, 1);                                               // strip any leading ' ' characters from the record

                while (record.size() > 0               &&
                      (record[record.size()-1] == '\r' ||
                       record[record.size()-1] == '\n' ||
                       record[record.size()-1] == ' '  )) record.erase(record.size()-1, 1);                                     // strip any trailing '\r', '\n' and ' ' characters from the record

                if (record.empty()) {
                    lineNo++;                                                                                                   // increment line number
                    continue;                                                                                                   // skip empty record
                }

                emptyRecord = false;                                                                                            // have non-empty record

                record = utils::ToUpper(record);                                                                                // upshift

                std::stringstream recordSS(record);                                                                             // open stream on record
                std::string token;
                while (std::getline(recordSS, token, ',')) {                                                                    // get token from record read

                    while (token.size() > 0 && token[0] == ' ') token.erase(0, 1);                                              // strip any leading ' ' characters from the token

                    while (token.size() > 0              &&
                          (token[token.size()-1] == '\r' ||
                           token[token.size()-1] == '\n' ||
                           token[token.size()-1] == ' ')) token.erase(token.size()-1, 1);                                       // strip any trailing '\r', '\n' and ' ' characters from the token

                    if (token.empty()) {                                                                                        // empty token?
                        SAY(ERR_MSG(ERROR::GRID_FILE_EMPTY_HEADER));                                                            // show error
                        continue;                                                                                               // next token
                    }

                    switch (_(token.c_str())) {                                                                                 // which column header?

                        case _("MASS_1")             : mass1++;             gridHeaders.push_back(token); break;                // Star 1 Mass
                        case _("MASS_2")             : mass2++;             gridHeaders.push_back(token); break;                // Star 2 Mass

                        case _("METALLICITY_1")      : metallicity1++;      gridHeaders.push_back(token); break;                // Star 1 Metallicity
                        case _("METALLICITY_2")      : metallicity2++;      gridHeaders.push_back(token); break;                // Star 2 Metallicity

                        case _("SEPARATION")         : separation++;        gridHeaders.push_back(token); break;                // Separation

                        case _("ECCENTRICITY")       : eccentricity++;      gridHeaders.push_back(token); break;                // Eccentricity

                        case _("PERIOD")             : period++;            gridHeaders.push_back(token); break;                // Period

                        case _("KICK_VELOCITY_1")    : kickVelocity1++;     gridHeaders.push_back(token); break;                // Star 1 Kick velocity

                        case _("KICK_THETA_1")       : kickTheta1++;        gridHeaders.push_back(token); break;                // Star 1 Kick theta

                        case _("KICK_PHI_1")         : kickPhi1++;          gridHeaders.push_back(token); break;                // Star 1 Kick phi

                        case _("KICK_MEAN_ANOMALY_1"): kickMeanAnomaly1++;  gridHeaders.push_back(token); break;                // Star 1 Kick mean anomaly

                        case _("KICK_VELOCITY_2")    : kickVelocity2++;     gridHeaders.push_back(token); break;                // Star 2 Kick velocity

                        case _("KICK_THETA_2")       : kickTheta2++;        gridHeaders.push_back(token); break;                // Star 2 Kick theta

                        case _("KICK_PHI_2")         : kickPhi2++;          gridHeaders.push_back(token); break;                // Star 2 Kick phi

                        case _("KICK_MEAN_ANOMALY_2"): kickMeanAnomaly2++;  gridHeaders.push_back(token); break;                // Star 2 Kick mean anomaly

                        default:                                                                                                // unknown column header
                            SAY(ERR_MSG(ERROR::GRID_FILE_UNKNOWN_HEADER) << " " << token);                                      // show error
                    }
                }

                if (mass1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Mass_1")                                       // no 'Mass_1'
                else if (mass1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Mass_1");                               // duplicate 'Mass_1'

                if (mass2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Mass_2")                                       // no 'Mass_2'
                else if (mass2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Mass_2");                               // duplicate 'Mass_2'

                if (metallicity1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Metallicity_1")                         // no 'Metallicity_1'
                else if (metallicity1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Metallicity_1");                 // duplicate 'Metallicity_1'

                if (metallicity2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Metallicity_2")                         // no 'Metallicity_2'
                else if (metallicity2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Metallicity_2");                 // duplicate 'Metallicity_2'

                if (eccentricity < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Eccentricity")                          // no 'Eccentricity'
                else if (eccentricity > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Eccentricity");                  // duplicate 'Eccentricity'

                if (separation < 1 && period < 1) {                                                                             // neither 'Separation' nor 'Period'
                    SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " One of {Separation, Period}");
                }
                else {
                    if (separation > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Separation");                       // duplicate 'Separation'
                    if (period > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Period");                               // duplicate 'Period'
                }

                if ((kickVelocity1 + kickTheta1 + kickPhi1 + kickMeanAnomaly1 + 
                     kickVelocity2 + kickTheta2 + kickPhi2 + kickMeanAnomaly2) > 0) {                                           // at least one kick* header present, so all are required

                    if (kickVelocity1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Velocity_1")                  // no 'Kick_Velocity_1'
                    else if (kickVelocity1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Velocity_1");          // duplicate 'Kick_Velocity_1'

                    if (kickTheta1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Theta_1")                        // no 'Kick_Theta_1'
                    else if (kickTheta1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Theta_1");                // duplicate 'Kick_Theta_1'

                    if (kickPhi1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Phi_1")                            // no 'Kick_Phi_1'
                    else if (kickPhi1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Phi_1");                    // duplicate 'Kick_Phi_1'

                    if (kickMeanAnomaly1 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Mean_Anomaly_1")           // no 'Kick_Mean_Anomaly_1'
                    else if (kickMeanAnomaly1 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Mean_Anomaly_1");   // duplicate 'Kick_Mean_Anomaly_1'

                    if (kickVelocity2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Velocity_2")                  // no 'Kick_Velocity_2'
                    else if (kickVelocity2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Velocity_2");          // duplicate 'Kick_Velocity_2'

                    if (kickTheta2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Theta_2")                        // no 'Kick_Theta_2'
                    else if (kickTheta2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Theta_2");                // duplicate 'Kick_Theta_2'

                    if (kickPhi2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Phi_2")                            // no 'Kick_Phi_2'
                    else if (kickPhi2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Phi_2");                    // duplicate 'Kick_Phi_2'

                    if (kickMeanAnomaly2 < 1) SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_HEADER) << " Kick_Mean_Anomaly_2")           // no 'Kick_Mean_Anomaly_2'
                    else if (kickMeanAnomaly2 > 1) SAY(ERR_MSG(ERROR::GRID_FILE_DUPLICATE_HEADER) << " Kick_Mean_Anomaly_2");   // duplicate 'Kick_Mean_Anomaly_2'
                }

                lineNo++;                                                                                                       // increment line number
            }
        }
    }

    // check we have all the headers we need, and in the right numbers
    if (!(mass1                == 1 && mass2        == 1                                  &&
          metallicity1         == 1 && metallicity2 == 1                                  &&
          separation           <= 1 && period       <= 1                                  &&
         (separation + period) >  0 && eccentricity == 1)                                 &&
        ((kickVelocity1 + kickTheta1 + kickPhi1 + kickMeanAnomaly1 + 
          kickVelocity2 + kickTheta2 + kickPhi2 + kickMeanAnomaly2) == 0                  ||
         (kickVelocity1 == 1 && kickTheta1 == 1 && kickPhi1 == 1 && kickMeanAnomaly1 == 1 && 
          kickVelocity2 == 1 && kickTheta2 == 1 && kickPhi2 == 1 && kickMeanAnomaly2 == 1))) {

        if (p_Grid.is_open()) p_Grid.close();                                                                                   // we don't - must have been a problem - close the grid file
    }

    return std::make_tuple(lineNo, gridHeaders);
}


/*
 * Read and parse the next record from the BSE grid file
 *
 * Expects the following units:
 * 
 *    Mass      : Msol
 *    Separation: AU
 *    Period    : Days
 *    Velocity  : kms^-1
 *    Theta, Phi: radians
 *    Anomaly   : radians (?)
 * 
 * 
 * Returns the following data values from the Grid file in the following order:
 *
 *    <Mass_1,
 *     Mass_2,
 *     Metallicity_1, 
 *     Metallicity_2, 
 *     Separation, 
 *     Eccentricity, 
 *     Kick_Velocity_1, 
 *     Kick_Theta_1, 
 *     Kick_Phi_1,
 *     Kick_Mean_Anomaly_1, 
 *     Kick_Velocity_2, 
 *     Kick_Theta_2, 
 *     Kick_Phi_2,
 *     Kick_Mean_Anomaly_2>
 *
 * If the user specifies Period rather than Separation, the separation is calculated using the masses and the orbital period
 * If the user specifies both Separation and Period, Separation is used in preference to Period
 *
 * Missing values are treated as zero (0.0) - a warning will be issued, and reading of the Grid file continues
 * (A value is considered missing only if there is a header for the column, but no data value in the column)
 * Invalid values are treated as errors - an error will be issued and reading of the Grid file stopped
 * Negative values for Mass, Metallicity, Separation and Eccentricity are treated as errors - an error will be issued and reading of the Grid file stopped
 *
 * JR todo: there are some hard-coded numbers here (e.g. size of p_Headers vector) that should be made constants one day
 * 
 * 
 * std::tuple<int, std::vector<double>> ReadBSEGridRecord(std::ifstream &p_Grid, const std::vector<std::string> p_GridHeaders, const int p_LineNo)
 *
 * @param   [IN/OUT]    p_Grid                  The Grid file (std::ifstream)
 * @param   [IN]        p_GridHeaders           Vector of header strings returned by OpenBSEGridFile()
 * @param   [IN]        p_LineNo                The line number of the next line to be read
 * @return                                      tuple containing the line number of the next line to be read, and a vector of data values
 */
std::tuple<int, std::vector<double>> ReadBSEGridRecord(std::ifstream &p_Grid, const std::vector<std::string> p_GridHeaders, const int p_LineNo) {

    bool error = false;

    std::vector<double> gridValues;                                                                                     // grid record values
    if (p_GridHeaders.size() == 6)
        gridValues = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    else
        gridValues = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};

    int lineNo = p_LineNo;                                                                                              // line number - for error messages

    bool emptyRecord = true;
    std::string record;
    while (!error && emptyRecord && std::getline(p_Grid, record)) {                                                     // read the next record of the file

        size_t hashPos = record.find("#");                                                                              // find first occurrence of "#"
        if (hashPos != std::string::npos) record.erase(hashPos, record.size() - hashPos);                               // if "#" found, prune it and everything after it (ignore comments)

        for (size_t pos = 0; pos < record.size(); pos++) if (record[pos] == '\t') record[pos] = ' ';                    // replace tab with space

        while (record.size() > 0 && record[0] == ' ') record.erase(0, 1);                                               // strip any leading ' ' characters from the record

        while (record.size() > 0               &&
              (record[record.size()-1] == '\r' ||
               record[record.size()-1] == '\n' ||
               record[record.size()-1] == ' '  )) record.erase(record.size()-1, 1);                                     // strip any trailing '\r', '\n' and ' ' characters from the record

        if (record.empty()) {
            lineNo++;                                                                                                   // increment line number
            continue;                                                                                                   // skip empty record
        }
        emptyRecord = false;                                                                                            // have non-empty record

        std::istringstream recordSS(record);                                                                            // open input stream on record

        double period = 0.0;

        size_t column = 0;                                                                                              // start at column 0
        std::string token;                                                                                              // token from record
        while (!error && std::getline(recordSS, token, ',')) {                                                          // get next token from record read

            while (token.size() > 0 && token[0] == ' ') token.erase(0, 1);                                              // strip any leading ' ' characters from the token

            while (token.size() > 0              &&
                  (token[token.size()-1] == '\r' ||
                   token[token.size()-1] == '\n' ||
                   token[token.size()-1] == ' '  )) token.erase(token.size()-1, 1);                                     // strip any trailing '\r', '\n' and ' ' characters from the token

            if (column >= p_GridHeaders.size()) {
                SAY(ERR_MSG(ERROR::GRID_FILE_EXTRA_COLUMN) << " ignored");                                              // show error
            }
            else {

                if (token.empty()) {                                                                                    // empty token?
                    token = "0.0";                                                                                      // yes - use 0.0
                    SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_DATA) << " at line " << lineNo << ": 0.0 used");               // show warning
                }

                double value;
                std::istringstream tokenSS(token);                                                                      // open input stream on token
                tokenSS >> value;                                                                                       // read double from token
                if (tokenSS.fail()) {                                                                                   // check for valid conversion
                    error = true;                                                                                       // set error flag
                    SAY(ERR_MSG(ERROR::GRID_FILE_INVALID_DATA) << "at line " << lineNo << ": " << token);               // show error
                }

                if (!error) {
                    std::string columnName = p_GridHeaders[column];                                                     // get column name for value
                    switch (_(columnName.c_str())) {                                                                    // which column?

                        case _("MASS_1"):                                                                               // Star 1 Mass
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[0] = value;                                                                 // no - proceed
                            break;

                        case _("MASS_2"):                                                                               // Star 2 Mass
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[1] = value;                                                                 // no - proceed
                            break;

                        case _("METALLICITY_1"):                                                                        // Star 1 Metallicity
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[2] = value;                                                                 // no - proceed
                            break;

                        case _("METALLICITY_2"):                                                                        // Star 2 Metallicity
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[3] = value;                                                                 // no - proceed
                            break;

                        case _("SEPARATION"):                                                                           // Separation
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[4] = value;                                                                 // no - proceed
                            break;

                        case _("ECCENTRICITY"):                                                                         // Eccentricity
                            if (value < 0.0) {                                                                          // value < 0?
                                error = true;                                                                           // yes - set error flag
                                SAY(ERR_MSG(ERROR::GRID_FILE_NEGATIVE_DATA) << " at line " << lineNo << ": " << token); // show error
                            }
                            else gridValues[5] = value;                                                                 // no - proceed
                            break;

                        case _("PERIOD")             : period         = value; break;                                   // Period

                        case _("KICK_VELOCITY_1")    : gridValues[6]  = value; break;                                   // Star 1 Kick velocity

                        case _("KICK_THETA_1")       : gridValues[7]  = value; break;                                   // Star 1 Kick theta

                        case _("KICK_PHI_1")         : gridValues[8]  = value; break;                                   // Star 1 Kick phi

                        case _("KICK_MEAN_ANOMALY_1"): gridValues[9]  = value; break;                                   // Star 1 Kick mean anomaly

                        case _("KICK_VELOCITY_2")    : gridValues[10] = value; break;                                   // Star 2 Kick velocity

                        case _("KICK_THETA_2")       : gridValues[11] = value; break;                                   // Star 2 Kick theta

                        case _("KICK_PHI_2")         : gridValues[12] = value; break;                                   // Star 2 Kick phi

                        case _("KICK_MEAN_ANOMALY_2"): gridValues[13] = value; break;                                   // Star 2 Kick mean anomaly

                        default:                                                                                        // unknown - this shouldn't happen
                            SAY(ERR_MSG(ERROR::GRID_FILE_UNKNOWN_HEADER) << " " << columnName);                         // show error
                    }
                }
            }
            column++;                                                                                                   // next column
        }

        if (!error) {
            if (column < p_GridHeaders.size()) {
                for (size_t c = column; c < p_GridHeaders.size(); c++) {
                    SAY(ERR_MSG(ERROR::GRID_FILE_MISSING_DATA) << " at line " << lineNo << ": 0.0 used");               // show warning
                }
            }

            if (gridValues[4] <= 0.0 && period > 0.0 && gridValues[0] > 0.0 && gridValues[1] > 0.0) {                   // already have separation? Have required values to calculate from period?
// JR: change the next line to a warning that can be suppressed            
//                SAY(ERR_MSG(ERROR::GRID_FILE_USING_PERIOD));                                                            // let the user know we're using Period
                gridValues[4] = utils::ConvertPeriodInDaysToSemiMajorAxisInAU(gridValues[0], gridValues[1], period);    // calculate separation from period
            }
        }
        lineNo++;                                                                                                       // increment line number
    }

    if (error) gridValues = {};                                                                                         // return empty vector if error occurred

    return std::make_tuple(lineNo, gridValues);
}


/*
 * Evolve binary stars
 *
 *
 * void EvolveBinaryStars()
 */
void EvolveBinaryStars() {

    EVOLUTION_STATUS evolutionStatus = EVOLUTION_STATUS::CONTINUE;

    auto wallStart = std::chrono::system_clock::now();                                                                      // start wall timer
    clock_t clockStart = clock();                                                                                           // start CPU timer

    if (!OPTIONS->Quiet()) {
        std::time_t timeStart = std::chrono::system_clock::to_time_t(wallStart);
        SAY("Start generating binaries at " << std::ctime(&timeStart));
    }

    AIS ais;                                                                                                                // Adaptive Importance Sampling (AIS)

    if (OPTIONS->AIS_ExploratoryPhase()) ais.PrintExploratorySettings();                                                    // print the selected options for AIS Exploratory phase in the beginning of the run
    if (OPTIONS->AIS_RefinementPhase() ) ais.DefineGaussians();                                                             // if we are sampling using AIS (step 2):read in gaussians

    std::ifstream grid;                                                                                                     // grid file
    std::vector<std::string> gridHeaders;                                                                                   // grid file headers

    int lineNo;                                                                                                             // grid file line number - for error messages
    if (!OPTIONS->GridFilename().empty()) {                                                                                 // have grid filename?
        std::tie(lineNo, gridHeaders) = OpenBSEGridFile(grid, OPTIONS->GridFilename());                                     // yes - read grid file headers
        if (!grid.is_open()) evolutionStatus = EVOLUTION_STATUS::ERROR;                                                     // failed to open/read grid file
    }

    // generate and evolve binaries
    int index    = 0;                                                                                                       // which binary
    int nBinaries = grid.is_open() ? 1 : OPTIONS->NBinaries();                                                              // assumption is OPTIONS->nBinaries > 0 JR: todo: should check options...

    BinaryStar *binary = nullptr;
    while (evolutionStatus == EVOLUTION_STATUS::CONTINUE && index < nBinaries) {

        if (!OPTIONS->GridFilename().empty()) {                                                                             // have grid filename?
            if (grid.is_open()) {                                                                                           // yes - grid file open?

                std::vector<double> gV;                                                                                     // yes - read the next record of grid values
                std::tie(lineNo, gV) = ReadBSEGridRecord(grid, gridHeaders, lineNo);                                        // read grid file record
                if (grid.fail() || gV.empty()) {                                                                            // read failed?
                    grid.close();                                                                                           // yes - EOF or error - stop reading, close file
                    if (gV.empty()) evolutionStatus = EVOLUTION_STATUS::STOPPED;                                            // if error, set error
                    else            evolutionStatus = EVOLUTION_STATUS::DONE;                                               // otherwise, set done
                }
                else {                                                                                                      // no
                    nBinaries++;                                                                                            // not done yet...
                    delete binary;
                    if (gridHeaders.size() == 6)
                        binary = new BinaryStar(ais, gV[0], gV[1], gV[2], gV[3], gV[4], gV[5], {}, {}, index);              // generate a binary according to specified values in grid file
                    else
                        binary = new BinaryStar(ais, gV[0], gV[1], gV[2], gV[3], gV[4], gV[5], 
                                                   { gV[6], gV[7], gV[8], gV[9] }, 
                                                   { gV[10], gV[11], gV[12], gV[13] }, index);                              // generate a binary according to specified values in grid file
                }
            }
            else evolutionStatus = EVOLUTION_STATUS::STOPPED;                                                               // must have been a problem
        }
        else {                                                                                                              // no grid file

            if (OPTIONS->IndividualSystem()) {                                                                              // user wants to create a binary with specified properties

                double separation = DEFAULT_INITIAL_DOUBLE_VALUE;                                                           // default is no separation...

                if (utils::Compare(OPTIONS->BinarySeparation(), 0.0) > 0) {                                                 // user specified separation
                    if (utils::Compare(OPTIONS->BinaryOrbitalPeriod(), 0.0) > 0) {                                          // ... and orbital period - oops
                        SHOW_WARN(ERROR::HAVE_SEPARATION_AND_PERIOD);                                                       // show warning
                    }
                    separation = OPTIONS->BinarySeparation();                                                               // use separation
                }
                else {                                                                                                      // user did not specify separation
                    if (utils::Compare(OPTIONS->BinaryOrbitalPeriod(), 0.0) <= 0) {                                         // ... or orbital period - oops
                        SHOW_WARN(ERROR::HAVE_NEITHER_SEPARATION_NOR_PERIOD);                                               // show warning
                    }
                    else {                                                                                                  // user specified orbital period - use it
                        separation = utils::ConvertPeriodInDaysToSemiMajorAxisInAU(OPTIONS->PrimaryMass(),
                                                                                   OPTIONS->SecondaryMass(),
                                                                                   OPTIONS->BinaryOrbitalPeriod());
                    }
                }

                delete binary;
                binary = new BinaryStar(ais,
                                        OPTIONS->PrimaryMass(),
                                        OPTIONS->SecondaryMass(),
                                        OPTIONS->InitialPrimaryMetallicity(),
                                        OPTIONS->InitialSecondaryMetallicity(),
                                        separation,
                                        OPTIONS->BinaryEccentricity(),
                                        {},
                                        {},
                                        index);                                                                             // generate a binary according to user-specified values
            }
            else {
                delete binary;
                binary = new BinaryStar(ais, index);                                                                        // generate a random binary according to the user options
            }
        }

        if (evolutionStatus == EVOLUTION_STATUS::CONTINUE) {                                                                // still good?

            EVOLUTION_STATUS binaryStatus = binary->Evolve(index);                                                          // evolve the binary

            // announce result of evolving the binary
            if (!OPTIONS->Quiet()) {
                if (OPTIONS->CHE_Option() == CHE_OPTION::NONE) {
                    SAY(index                                      << ": "  <<
                        EVOLUTION_STATUS_LABEL.at(binaryStatus)    << ": "  <<
                        STELLAR_TYPE_LABEL.at(binary->Star1Type()) << " + " <<
                        STELLAR_TYPE_LABEL.at(binary->Star2Type())
                    );
                }
                else {
                    SAY(index                                             << ": "    <<
                        EVOLUTION_STATUS_LABEL.at(binaryStatus)           << ": ("   <<
                        STELLAR_TYPE_LABEL.at(binary->Star1InitialType()) << " -> "  <<
                        STELLAR_TYPE_LABEL.at(binary->Star1Type())        << ") + (" <<
                        STELLAR_TYPE_LABEL.at(binary->Star2InitialType()) << " -> "  <<
                        STELLAR_TYPE_LABEL.at(binary->Star2Type())        <<  ")"
                    );
                }
            }


            if (OPTIONS->AIS_ExploratoryPhase() && ais.ShouldStopExploratoryPhase(index)) {                                 // AIS says should stop simulation?
                SHOW_WARN(ERROR::BINARY_SIMULATION_STOPPED, EVOLUTION_STATUS_LABEL.at(EVOLUTION_STATUS::AIS_EXPLORATORY));  // yes - show warning
                break;                                                                                                      // ... and stop
            }

            if (!LOGGING->CloseStandardFile(LOGFILE::BSE_DETAILED_OUTPUT)) {                                                // close detailed output file
                SHOW_WARN(ERROR::FILE_NOT_CLOSED);                                                                          // close failed - show warning
                evolutionStatus = EVOLUTION_STATUS::STOPPED;                                                                // this will cause problems later - stop evolution
            }
        }

        ERRORS->Clean();                                                                                                    // clean the dynamic error catalog

        index++;                                                                                                            // next...
    }
    delete binary;

    if (evolutionStatus == EVOLUTION_STATUS::CONTINUE && index >= nBinaries) evolutionStatus = EVOLUTION_STATUS::DONE;      // set done


    // announce result
    if (!OPTIONS->Quiet()) {
        if (evolutionStatus != EVOLUTION_STATUS::CONTINUE) {
            SAY("\n" << EVOLUTION_STATUS_LABEL.at(evolutionStatus));
        }
    }

    // close BSE logfiles
    // don't check result here - let log system handle it

    (void)LOGGING->CloseAllStandardFiles();

    if (!OPTIONS->Quiet()) {

        double cpuSeconds = (clock() - clockStart) / (double) CLOCKS_PER_SEC;                                               // stop CPU timer and calculate seconds

        auto wallEnd = std::chrono::system_clock::now();                                                                    // stope wall timer
        std::time_t timeEnd = std::chrono::system_clock::to_time_t(wallEnd);                                                // get end time and date

        SAY("\nEnd generating binaries at " << std::ctime(&timeEnd));
        SAY("Clock time = " << cpuSeconds << " CPU seconds");


        std::chrono::duration<double> wallSeconds = wallEnd - wallStart;                                                    // elapsed seconds

        int wallHH = (int)(wallSeconds.count() / 3600.0);                                                                   // hours
        int wallMM = (int)((wallSeconds.count() - ((double)wallHH * 3600.0)) / 60.0);                                       // minutes
        int wallSS = (int)(wallSeconds.count() - ((double)wallHH * 3600.0) - ((double)wallMM * 60.0));                      // seconds

        SAY("Wall time  = " << wallHH << ":" << wallMM << ":" << wallSS << " (hh:mm:ss)");
    }
}


/*
 * COMPAS main program
 *
 * Does some housekeeping:
 *
 * - starts the Options service (program options)
 * - starts the Log service (for logging and debugging)
 * - starts the Rand service (random number generator)
 *
 * Then evolves either a single or binary star (single star only at the moment...)
 *
 */
int main(int argc, char * argv[]) {

    COMMANDLINE_STATUS programStatus = OPTIONS->Initialise(argc, argv);                 // Get the program options from the commandline

    if (programStatus == COMMANDLINE_STATUS::CONTINUE) {

        // start the logging service
        LOGGING->Start(OPTIONS->OutputPathString(),                                     // location of logfiles
                       OPTIONS->LogfileNamePrefix(),                                    // prefix for logfile names
                       OPTIONS->LogLevel(),                                             // log level - determines (in part) what is written to log file
                       OPTIONS->LogClasses(),                                           // log classes - determines (in part) what is written to log file
                       OPTIONS->DebugLevel(),                                           // debug level - determines (in part) what debug information is displayed
                       OPTIONS->DebugClasses(),                                         // debug classes - determines (in part) what debug information is displayed
                       OPTIONS->DebugToFile(),                                          // should debug statements also be written to logfile?
                       OPTIONS->ErrorsToFile(),                                         // should error messages also be written to logfile?
                       DELIMITERValue.at(OPTIONS->LogfileDelimiter()));                 // log record field delimiter

        utils::SplashScreen();                                                          // announce ourselves

        if (!LOGGING->Enabled()) programStatus = COMMANDLINE_STATUS::LOGGING_FAILED;    // logging failed to start
        else {

            RAND->Initialise();                                                         // initialise the random number service

            if(OPTIONS->SingleStar()) {                                                 // Single star?
                EvolveSingleStars();                                                    // yes - evolve single stars
            }
            else {                                                                      // no - binary
                EvolveBinaryStars();                                                    // evolve binary stars
            }

            RAND->Free();                                                               // release gsl dynamically allocated memory

            LOGGING->Stop();                                                            // stop the logging service

            programStatus = COMMANDLINE_STATUS::SUCCESS;                                // set program status, and...
        }
    }

    return static_cast<int>(programStatus);                                             // we're done
}

