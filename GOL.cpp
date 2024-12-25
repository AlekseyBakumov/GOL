#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <queue>
#ifdef _WIN32
#include <windows.h>
#define CLEAR_SCREEN system("cls");
#else
#include <unistd.h>
#define CLEAR_SCREEN system("reset");
#endif

#define ACTIVE_CELL_CHAR '#'
#define DEAD_CELL_CHAR ' '
#define END_OF_FIELD_CHAR '|'

#define SLEEP_TIME_MS 40
#define DEFAULT_FIELD_SIZE 20,60


// Cross-platform sleep function
void sleepcp(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// Returns first word in string
std::string getword(std::string line)
{
    int pos = line.find(" ");
    if (pos >= line.size())
        return line;
    return line.substr(0, line.find(" "));
}

/// <summary>
///  Field class, contains information about cells.
///  Supports get, set by coords(x,y) and draw field in console
/// </summary>
class Field 
{
private:
    bool** _field;
    int n, m;

    // Initialize field NxM
    void createField(int n, int m)
    {
        this->n = n;
        this->m = m;
        _field = new bool* [n];
        for (int i = 0; i < n; i++)
            _field[i] = new bool[m];
    }

public:
#pragma region Constructors

    // Default field
    Field()
    {
        createField(10, 10);
    }
    // Field with size = NxM
    Field(int n, int m)
    {
        createField(n, m);
    }

#pragma endregion

    void DefaultPreset() 
    {
        Clear();

        setAt(0, -1, true);
        setAt(1, 0, true);
        setAt(-1, 1, true);
        setAt(0, 1, true);
        setAt(1, 1, true);
    }

    void Clear()
    {
        for (int i = 0; i < n; i++)
            for (int j = 0; j < m; j++)
                _field[i][j] = false;
    }

    void Draw()
    {
        for (int i = 0; i < n; i++)
        {
            for (int j = 0; j < m; j++)
                std::cout << (getAt(i,j) ? ACTIVE_CELL_CHAR : DEAD_CELL_CHAR);
            std::cout << END_OF_FIELD_CHAR << std::endl;
        }

    }

#pragma region IndexationUtils

    // Block of utility functions for
    // array indexation

    // Normalization provides closure of field
    // e.g. (x = -1) => (x = N)
    //      (x =  N) => (x = 0)
    // Same for y

    int getN()        { return n; }
    int getM()        { return m; }
    int normalizeX(int x, bool is_y = false) 
    {
        int border = n;
        int sign = 1;
        if (is_y) border = m;
        if (x < 0) sign = -1;

        int res = x - border * (int)(x / border);
        if (res < 0) res += border;
        return res;
    }
    int normalizeY(int y) { return normalizeX(y, true); }
    bool getAt(int x, int y) { return _field[normalizeX(x)][normalizeY(y)]; }
    void setAt(int x, int y, bool val) { _field[normalizeX(x)][normalizeY(y)] = val; }
    //bool** getField() { return _field; }

#pragma endregion
};

// Struct that represents cell operation
// Means: "set <val> at <x,y>"
typedef struct CellOp_s
{
    int x, y;
    bool val;
} CellOp;

/// <summary>
/// This class is used as a part of the logic
/// to read and parse files with presets
/// </summary>
class PresetParser
{

private:
    std::string _inputFile;

    std::string _presetName;
    std::string _presetComment;
    int b, s;
    bool parsed = false;

    // Choosing a parser for parameter string
    // marked with # at the beginning of the line
    // Returns true if parameter is recognized and parsed
    // false - otherwise
    bool parseParameter(std::string line)
    {
        switch (line[1])
        {
        case 'R':
            return parseR(line);
            break;
        case 'N':
            return parseN(line);
            break;
        }
        return false;
    }

    // Parser for R parameter
    bool parseR(std::string line)
    {
#ifdef _WIN32
        int ret = sscanf_s(line.c_str(), "#R B%d/S%d", &b, &s);
#else
        int ret = sscanf(line.c_str(), "#R B%d/S%d", &b, &s);
#endif
        return ret == 2;
    }
    // Parser for N parameter
    bool parseN(std::string line)
    {
        _presetComment = line.substr(3);
        return _presetComment.size() > 0;
    }
    // Parser for active cell
    bool parseCell(std::string line, std::queue<CellOp*>* ops_q)
    {
        int x, y;
        std::stringstream  linestream(line);
        linestream >> x >> y;
        if (!linestream) return false;
        CellOp* op = new CellOp;
        op->x = x;
        op->y = y;
        op->val = true;
        ops_q->push(op);
        return true;
    }

public:
    PresetParser(char* str)       { _inputFile = std::string(str); }
    PresetParser(std::string str) { _inputFile = str;              }
    void SetFile(char* str)       { _inputFile = std::string(str); }
    void SetFile(std::string str) { _inputFile = str;              }

    // Start parsing file
    // Filling queue with operations, allowing Logic class
    // to restore world from preset in file
    // Also sets preset paremeters such as name, comment and B/S
    // Note that this parameters are stored inside this class
    // and needs to be read separately from queue
    void Parse(std::queue<CellOp*>* ops_q)
    {
        std::ifstream infile(_inputFile);
        if (!infile) throw std::invalid_argument("Input file not found");
        std::string line;
        int count = 0;

        // First line is a name
        std::getline(infile, _presetName);

        while (std::getline(infile, line)) {
            count++;
            bool result;
            if (line[0] == '#') result = parseParameter(line);
            else                result = parseCell(line, ops_q);
            if (!result) std::cout << "[Line " << count << "] " 
                                   << "Failed to parse: " << line << std::endl;
        }

        parsed = true;
    }

    int GetB() { return b; }
    int GetS() { return s; }

    std::string GetComment() { return _presetComment; }
    std::string GetName()    { return _presetName;    }
};

/// <summary>
/// Performs all operations over cells and updates state
/// of the game by Tick()
/// </summary>
class Logic 
{
private:
    Field* _field_ptr = nullptr;
    std::queue<CellOp*> ops_q;
    int b, s;

    // Sum of active neighbours for cell at (x,y)
    int activeCellSum(int x, int y)
    {
        int sum = 0;
        for (int i = x - 1; i <= x + 1; i++)
        {
            for (int j = y - 1; j <= y + 1; j++)
            {
                if (i == x && j == y) continue;
                sum += (_field_ptr->getAt(i, j)) ? 1 : 0;
            }
        }
        return sum;
    }
    // Check sum of neighbour active cells for birth
    // If s == true, it checks S (survival) instead
    // For S better use checkS(sum)
    bool checkB(int sum, bool check_s = false)
    {
        int _b = b;
        if (check_s) _b = s;

        while (_b > 0)
        {
            if (_b % 10 == sum)
                return true;
            _b /= 10;
        }

        return false;
    }
    // Check sum of neighbour active cells for survival
    bool checkS(int sum)
    {
        return checkB(sum, true);
    }
    // Push new cell operation into queue
    void pushOp(int x, int y, bool val)
    {
        CellOp* op = new CellOp;
        op->x = x;
        op->y = y;
        op->val = val;
        ops_q.push(op);
    }
    // Scans entire field and build operation queue
    // that can transform field to next iteration
    void scanField()
    {
        for (int i = 0; i < _field_ptr->getN(); i++)
        {
            for (int j = 0; j < _field_ptr->getM(); j++)
            {
                int cellSum  = activeCellSum(i,j);
                bool alive   = _field_ptr->getAt(i, j);
                bool birth   = checkB(cellSum);
                bool survive = checkS(cellSum);

                bool newState = (alive && survive) || birth;
                if (newState != alive) pushOp(i, j, newState);
            }
        }
    }
    // Applies cell operations listed in ops_q
    // and updates field
    void applyCellOpsQueue()
    {
        for (; !ops_q.empty(); ops_q.pop())
        {
            CellOp* op = ops_q.front();
            _field_ptr->setAt(op->x, op->y, op->val);
            delete op;
        }
    }

    // For debug. Prints map of active neighbours
    void printCellSums()
    {
        for (int i = 0; i < _field_ptr->getN(); i++)
        {
            for (int j = 0; j < _field_ptr->getM(); j++)
                std::cout << activeCellSum(i, j);
            std::cout << std::endl;
        }
    }

public:
    // Default logic B3/S23
    Logic(Field* field)
    {
        b = 3;
        s = 23;
        _field_ptr = field;
    }
    // Logic with Bb/Ss parameters
    // B and S are represented as integer
    // e.g. B123 => b = 123
    //      B45  => b = 45
    //      S140 => s = 140
    // WARNING: 0 in B/S parameters must NOT be first digit
    // Multiple same digits are counted as one: 1223 = 123
    Logic(Field* field, int b, int s)
    {
        this->b = b;
        this->s = s;
        _field_ptr = field;
    }
    void SetField(Field* field)
    {
        _field_ptr = field;
    }
    void Tick()
    {
        scanField();
        applyCellOpsQueue();
    }
    void DrawField() { _field_ptr->Draw(); }
    int GetFieldHeight() { return _field_ptr->getN(); }
    int GetFieldWidth() { return _field_ptr->getM(); }

    void LoadPreset(PresetParser* prepar)
    {
        _field_ptr->Clear();
        prepar->Parse(&ops_q);
        this->b = prepar->GetB();
        this->s = prepar->GetS();
        applyCellOpsQueue();
    }

    void LoadDefault()
    {
        _field_ptr->Clear();
        _field_ptr->DefaultPreset();
    }
};

// Struct for ModeSelector class
// Contains passed parameters from UserInterfaceWrap class
typedef struct ModeContext_s
{
    Logic** logic;
    PresetParser** prepar;
    std::string inputFile;
} ModeContext;

// Strategy abstract class for selecting different app mode
class ModeSelector
{
public:
    virtual void ConfigLogic(ModeContext context) = 0;
};

// Default mode, loading default map preset
// Ignores input file
// Sets prepar attribute as nullptr
class DefaultMode : public ModeSelector
{
private:

public:
    virtual void ConfigLogic(ModeContext context)
    {
        Field* f = new Field(DEFAULT_FIELD_SIZE);
        Logic* l = new Logic(f, 3, 23);
        l->LoadDefault();
        *(context.logic) = l;
        *(context.prepar) = nullptr;
    }
};

// Mode with loading map preset from input file
class LoadFileMode : public ModeSelector
{
private:

public:
    virtual void ConfigLogic(ModeContext context)
    {
        Field* f = new Field(DEFAULT_FIELD_SIZE);
        Logic* l = new Logic(f, 3, 23);
        PresetParser* p = new PresetParser(context.inputFile);
        l->LoadPreset(p);
        *(context.logic) = l;
        *(context.prepar) = p;
    }
};

class UserInterfaceWrap
{
private:
    Logic* _logic;
    PresetParser* _prepar;
    int _ticks = 0;
    bool _help = false;
    std::string _dump_file = std::string("");

    // Error indicator for commands
    // 0 - no errors
    // 1 - tick error
    // 2 - dump error
    // 3 - help error
    // 4 - exit error
    int _errNo = 0;
    std::string _error_msg = std::string("");


    // Set game mode using ModeSelector and loading file by name
    void setMode(ModeSelector* mode, std::string inputFile)
    {
        mode->ConfigLogic(ModeContext{ &_logic, &_prepar, inputFile });
    }
    // Draw map
    void drawField()
    {
        _logic->DrawField();
        for (int i = 0; i < _logic->GetFieldWidth(); i++)
            std::cout << "=";
        std::cout << END_OF_FIELD_CHAR << std::endl;
    }
    // Draw info box
    void drawInfo()
    {
        std::cout << "[INFO]----------------------------" << std::endl;
        if (_prepar == nullptr)
        {
            std::cout << "Default loaded preset" << std::endl;
        }
        else
        {
            std::cout << "Name: " << _prepar->GetName() << std::endl;
        }
        if (_errNo != 0)
        {
            std::cout << "[Error No. " << _errNo << "]:" << _error_msg << std::endl;
            _errNo = 0;
        }
    }
    // Draw box with user input and help
    void drawUserInput(bool help = false)
    {
        std::cout << "[INPUT]---------------------------" << std::endl;
        if (help)
        {
            std::cout << "Type \"tick\" <n> to advance game on n ticks." << std::endl;
            std::cout << "Type \"dump\" <file> to save state in file." << std::endl;
            std::cout << "Type \"exit\" to end game." << std::endl;
        }
        else std::cout << "Type \"help\" to view commands." << std::endl;
        std::cout << "Input command: ";
    }

    void dumpFile()
    {
        _dump_file = std::string("");
        std::cout << "Dump not yet implemented." << std::endl;
    }

    void ticks()
    {
        while (_ticks > 0)
        {
            _logic->Tick();
            CLEAR_SCREEN
            drawField();
            std::cout << "Remained ticks = " << --_ticks << std::endl;
            sleepcp(SLEEP_TIME_MS);
        }
    }

    void parseUInput(std::string line)
    {
        std::string word = getword(line);
        if (word == std::string("dump"))
        {
            if (line.size() > 5)
            {
                std::string arg = line.substr(5);
                _dump_file = arg;
            }
            else
            {
                _errNo = 2;
                _error_msg = std::string("Command \"dump\" requires argument");
            }
                
        }
        else if (word == std::string("tick"))
        {
            if (line.size() > 4)
            {
                std::string arg = line.substr(5);
                try
                {
                    _ticks = std::stoi(arg);
                }
                catch (const std::exception&)
                {
                    _errNo = 1;
                    _error_msg = std::string("Invalid argument for \"tick\": ") + std::string(arg);
                    _ticks = 0;
                }
            }
            else
            {
                _ticks = 1;
            }
        }
        else if (word == std::string("exit"))
        {
            if (line.size() > 4)
            {
                _errNo = 4;
                _error_msg = std::string("Command \"exit\" takes no arguments");
            }
            else
            {
                std::cout << "Closing game." << std::endl;
                exit(0);
            }
        }
        else if (word == std::string("help"))
        {
            _help = true;
            if (line.size() > 4)
            {
                _errNo = 3;
                _error_msg = std::string("Command \"help\" takes no arguments");
            }
        }
    }

public:
    UserInterfaceWrap(ModeSelector* mode, std::string inputFile)
    {
        std::cout << "Loading..." << std::endl;
        setMode(mode, inputFile);
        std::cout << "Complete." << std::endl;
    }

    void Start()
    {
        while (true)
        {
            std::string input_line;

            ticks();
            CLEAR_SCREEN
            drawField();
            drawInfo();
            if (!_dump_file.empty())
                dumpFile();
            drawUserInput(_help);
            _help = false;

            std::getline(std::cin, input_line);
            parseUInput(input_line);
        }
        
    }

    void DrawAll()
    {
        drawField();
        drawInfo();
        drawUserInput();
    }

};

int main()
{
    //ModeSelector* mode = new DefaultMode();
    ModeSelector* mode = new LoadFileMode();
    UserInterfaceWrap* UI = new UserInterfaceWrap(mode, "expl.txt");
    UI->Start();
    return 0;
}
