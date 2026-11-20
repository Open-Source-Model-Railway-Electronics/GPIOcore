// gpiocore_fw_host: the real GPIOcore firmware (software/GPIOcore) running on the PC.
//
// The GUI's simulator (src/simulateddevice.cpp) starts this program and talks to it through
// stdin / stdout in small frames:   <type> <length lo> <length hi> <payload ...>
//
// GUI -> firmware
//   'S' <bytes>                      serial bytes to the board (UART RX)
//   'X' <command> <payload>          run one protocol message through messageReceived() right away,
//                                    replies dropped (sets up the board); answered with 'x'
//   'A' <pin> <value H> <value L>    voltage on a pin, 0-1023 (A0-A7 = 14-21; buttons: 0 = pressed)
//   'D' <addr H> <addr L> <dir> <power>   DCC accessory packet (notifyDccAccTurnoutOutput)
//   'E' <addr H> <addr L> <value>    DCC extended accessory packet (notifyDccSigOutputState)
//   'L' <addr H> <addr L> <speed> <forward>   DCC loco speed, 128 steps (notifyDccSpeed)
//   'F' <addr H> <addr L> <F0-F4>    DCC functions F0-F4 (notifyDccFunc, FN_0_4)
//   'H' <baud / 2400>                a fake HC-05 in AT mode on D0/D1 that answers OK at that baud (0 = none)
//   'T' <ms lo> <ms hi>              the clock jumps ahead (tests: no waiting for the 5 s serial window)
//
// firmware -> GUI (state frames come before the serial bytes of the same loop() round)
//   'R' <version>                    setup() done
//   'S' <bytes>                      serial bytes from the board (UART TX at 115200 baud)
//   'x' <command> <nack>             result of an 'X' frame (0 = ACK)
//   'O' <16 bytes>                   an output changed (sketch.cpp fwOutputState)
//   'I' <8 + 4 n bytes>              an input or its route changed (fwInputState)
//   'B' <type> <flags> <delay> <name>     board settings changed
//   'G' <configSession> <serialIsIO> <configMode>
//   'P' <22 x mode, level, PWM, servo angle>   pins changed
//   'h' <text>                       an AT command the fake HC-05 received (without CR LF)
//
// The program ends when stdin closes. Options: --eeprom <file>  keep the EEPROM in a file.
// loop() runs continuously like on the board; when there is nothing to do the program sleeps ~1 ms
// between rounds of 32 loop() calls (two passes over the 16 outputs).

#include "hostapi.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <timeapi.h>
#else
#include <unistd.h>
#endif

extern const char *version;      // src/version.h, defined in GPIOcore.ino

// ── Clock ─────────────────────────────────────────────────────────────────────
using Clock = std::chrono::steady_clock;
static const Clock::time_point startTime = Clock::now();
static uint64_t delayOffsetUs = 0;

static uint64_t elapsedUs()
{
    return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - startTime).count())
         + delayOffsetUs;
}

uint32_t hostMillis()          { return uint32_t(elapsedUs() / 1000); }
uint32_t hostMicros()          { return uint32_t(elapsedUs()); }
void     hostDelay(uint32_t ms) { delayOffsetUs += uint64_t(ms) * 1000; }

// ── stdin reader ──────────────────────────────────────────────────────────────
static std::mutex              inMutex;
static std::condition_variable inSignal;
static std::vector<uint8_t>    inBytes;
static bool                    inEof = false;

static void readStdin()
{
    uint8_t buf[4096];
    for (;;)
    {
#ifdef _WIN32
        const int n = _read(0, buf, sizeof(buf));
#else
        const int n = int(read(0, buf, sizeof(buf)));
#endif
        std::lock_guard<std::mutex> lock(inMutex);
        if (n <= 0) { inEof = true; inSignal.notify_one(); return; }
        inBytes.insert(inBytes.end(), buf, buf + n);
        inSignal.notify_one();
    }
}

// ── stdout ────────────────────────────────────────────────────────────────────
static std::vector<uint8_t> outBytes;

static void send(char type, const uint8_t *data, size_t n)
{
    outBytes.push_back(uint8_t(type));
    outBytes.push_back(uint8_t(n & 0xFF));
    outBytes.push_back(uint8_t(n >> 8));
    outBytes.insert(outBytes.end(), data, data + n);
}

static void flushOut()
{
    if (outBytes.empty()) return;
    fwrite(outBytes.data(), 1, outBytes.size(), stdout);
    fflush(stdout);
    outBytes.clear();
}

// ── State reports: only what changed ──────────────────────────────────────────
static std::vector<uint8_t> lastOutput[16], lastInput[16], lastBoard, lastGlobals, lastPins;
static uint32_t lastEepromWrites = ~0u;

static void report(char type, std::vector<uint8_t> &last, const uint8_t *now, int n)
{
    if (int(last.size()) == n && memcmp(last.data(), now, size_t(n)) == 0) return;
    last.assign(now, now + n);
    send(type, now, size_t(n));
}

// After an 'X' frame everything is reported again: the GUI has guessed the new state already, and
// the firmware may have refused it or changed it back in the same round.
static void reportAll()
{
    for (auto &o : lastOutput) o.clear();
    for (auto &i : lastInput)  i.clear();
    lastBoard.clear();
    lastEepromWrites = ~0u;
}

static void reportChanges()
{
    uint8_t buf[256];
    for (int i = 0; i < 16; ++i)
        report('O', lastOutput[i], buf, fwOutputState(i, buf));
    if (hostEepromWrites() != lastEepromWrites)       // inputs, routes and board live in the EEPROM
    {
        lastEepromWrites = hostEepromWrites();
        for (int i = 0; i < 16; ++i)
            report('I', lastInput[i], buf, fwInputState(i, buf));
        report('B', lastBoard, buf, fwBoardState(buf));
    }
    report('G', lastGlobals, buf, fwGlobals(buf));
    hostPinState(buf);
    report('P', lastPins, buf, HostPins * 4);
    char line[64];
    while (hostHc05Take(line, sizeof(line)))
        send('h', reinterpret_cast<const uint8_t *>(line), strlen(line));

    uint8_t serial[4096];
    while (size_t n = hostSerialTake(serial, sizeof(serial)))
        send('S', serial, n);
    while (size_t n = hostLinkTake(serial, sizeof(serial)))
        send('K', serial, n);
}

// ── Frames from the GUI ───────────────────────────────────────────────────────
struct Frame
{
    char                 type;
    std::vector<uint8_t> data;
};

static std::vector<uint8_t> parseBuf;
static std::deque<Frame>    frames;

static void parseFrames()
{
    size_t pos = 0;
    while (parseBuf.size() - pos >= 3)
    {
        const size_t n = size_t(parseBuf[pos + 1]) | size_t(parseBuf[pos + 2]) << 8;
        if (parseBuf.size() - pos < 3 + n) break;
        Frame f;
        f.type = char(parseBuf[pos]);
        f.data.assign(parseBuf.begin() + long(pos) + 3, parseBuf.begin() + long(pos + 3 + n));
        frames.push_back(std::move(f));
        pos += 3 + n;
    }
    parseBuf.erase(parseBuf.begin(), parseBuf.begin() + long(pos));
}

static uint8_t at(const Frame &f, size_t i) { return i < f.data.size() ? f.data[i] : 0; }

// Runs the frames in order. A control frame waits until the firmware has read the serial bytes
// before it, so the order the GUI wrote things in is kept.
static void handleFrames()
{
    while (!frames.empty())
    {
        Frame &f = frames.front();
        if (f.type != 'S' && hostSerialPending() > 0)
            return;
        const uint16_t address = uint16_t(at(f, 0) << 8 | at(f, 1));
        switch (f.type)
        {
        case 'S': hostSerialPush(f.data.data(), f.data.size()); break;
        case 'X':
            if (!f.data.empty())
            {
                const uint8_t result[2] = { f.data[0], fwExecute(f.data.data(), int(f.data.size())) };
                send('x', result, 2);
                reportAll();
            }
            break;
        case 'A': hostSetLevel(at(f, 0), at(f, 1) << 8 | at(f, 2)); break;
        case 'D': fwDccAccessory(address, at(f, 2), at(f, 3)); break;
        case 'E': fwDccExtended(address, at(f, 2)); break;
        case 'L': fwDccSpeed(address, at(f, 2), at(f, 3)); break;
        case 'F': fwDccFunctions(address, at(f, 2)); break;
        case 'H': hostHc05(uint32_t(at(f, 0)) * 2400); break;
        case 'T': hostDelay(uint32_t(at(f, 0) | at(f, 1) << 8)); break;
        default:  fprintf(stderr, "gpiocore_fw_host: unknown frame '%c'\n", f.type); break;
        }
        frames.pop_front();
    }
}

// ── EEPROM file (optional) ────────────────────────────────────────────────────
static std::string eepromFile;

static void loadEeprom()
{
    if (eepromFile.empty()) return;
    if (FILE *f = fopen(eepromFile.c_str(), "rb"))
    {
        const size_t n = fread(hostEeprom(), 1, 1024, f);
        fclose(f);
        if (n != 1024) memset(hostEeprom(), 0xFF, 1024);
    }
}

static void saveEeprom()
{
    if (eepromFile.empty()) return;
    if (FILE *f = fopen(eepromFile.c_str(), "wb"))
    {
        fwrite(hostEeprom(), 1, 1024, f);
        fclose(f);
    }
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (!strcmp(argv[i], "--eeprom") && i + 1 < argc)
            eepromFile = argv[++i];
        else
        {
            fprintf(stderr, "usage: %s [--eeprom <file>]   (talks to the GPIOcore GUI on stdin / stdout)\n", argv[0]);
            return 2;
        }
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    timeBeginPeriod(1);                       // 1 ms sleeps instead of 15.6 ms
#endif

    loadEeprom();
    fwSetup();
    send('R', reinterpret_cast<const uint8_t *>(version), strlen(version));
    reportChanges();
    flushOut();

    std::thread(readStdin).detach();

    uint32_t savedWrites = hostEepromWrites();
    uint32_t savedAt     = hostMillis();
    for (;;)
    {
        bool eof;
        {
            std::lock_guard<std::mutex> lock(inMutex);
            parseBuf.insert(parseBuf.end(), inBytes.begin(), inBytes.end());
            inBytes.clear();
            eof = inEof;
        }
        parseFrames();
        handleFrames();

        for (int i = 0; i < 32; ++i)
            fwLoop();

        reportChanges();
        flushOut();

        if (eof && frames.empty())
            break;
        if (hostEepromWrites() != savedWrites && hostMillis() - savedAt > 1000)
        {
            saveEeprom();
            savedWrites = hostEepromWrites();
            savedAt     = hostMillis();
        }
        if (hostSerialPending() == 0 && frames.empty())
        {
            std::unique_lock<std::mutex> lock(inMutex);
            inSignal.wait_for(lock, std::chrono::milliseconds(1), [] { return !inBytes.empty() || inEof; });
        }
    }
    saveEeprom();
    return 0;
}
