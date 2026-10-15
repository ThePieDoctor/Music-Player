// Music Player Application
// Author [Evan Barrett]

#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <iomanip>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

// I used the assistance of AI to implement non-blocking keyboard input for the Music Player so the user can interact with it while it's playing. I was not sure how to code or implement this without AI assistance.
int _kbhit() {
    struct termios oldt, newt;
    int ch, oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if (ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
}
int _getch() { return getchar(); }
#endif

namespace fs = std::filesystem;

// Represents an individual track, including duration and formatting.
class Track {
private:
    std::string _title;
    std::string _path;
    int _durationSeconds;

    // Estimates duration in seconds based on a 320 kbps bitrate. All the mp3 songs I download have this bitrate. 
    // The duration of songs will be calculated differently (incorrectly) if the mp3 files have a different bitrate
    int EstimateDuration(const fs::path& filepath) {
        try {
            uintmax_t bytes = fs::file_size(filepath);
            // If you know the bitrate of your mp3 files, replace 40000 with the appropriate value
            int seconds = static_cast<int>(bytes / 40000); // 320 kbps approximation
            return (seconds > 0) ? seconds : 180;
        } catch (...) {
            return 180; // default 3 minutes if unreadable
        }
    }

public:
    Track() : _title(""), _path(""), _durationSeconds(0) {}
    // Initializes a track with its file path, title, and estimated duration.
    Track(const fs::path& filepath) {
        _path = filepath.string();
        _title = filepath.filename().stem().string();
        _durationSeconds = EstimateDuration(filepath);
    }
    
    std::string GetTitle() const { return _title; }
    std::string GetPath() const { return _path; }
    int GetDurationSeconds() const { return _durationSeconds; }

    // Formats time in MM:SS format
    static std::string FormatTime(int totalSec) {
        int m = totalSec / 60;
        int s = totalSec % 60;
        return (m < 10 ? "0" : "") + std::to_string(m) + ":" + (s < 10 ? "0" : "") + std::to_string(s);
    }
};

// Playlist manages heap allocation, indexing, and track storage.
class Playlist {
private:
    Track* _tracks;
    size_t _capacity;
    size_t _currentIndex;
// Dynamic heap allocation with a max capacity of 50 tracks in the playlist.
public:
    Playlist() : _tracks(nullptr), _capacity(0), _currentIndex(0) {}

    // Deletes track buffer to prevent memory leaks
    ~Playlist() {
        delete[] _tracks;
        _tracks = nullptr;
    }

    // Prevent unintentional copies
    Playlist(const Playlist&) = delete;
    Playlist& operator=(const Playlist&) = delete;
    
    bool LoadFromDirectory(const std::string& directoryPath, size_t maxTracks = 50) {
        if (!fs::exists(directoryPath) || !fs::is_directory(directoryPath)) {
            return false;
        }

        // Collect all MP3 files in the directory
        std::vector<fs::path> mp3Files;
        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mp3") {
                mp3Files.push_back(entry.path());
            }
        }

        if (mp3Files.empty()) return false;

        // Shuffle tracks
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(mp3Files.begin(), mp3Files.end(), g);

        // Dynamic heap allocation for track buffer
        delete[] _tracks;
        _capacity = std::min(maxTracks, mp3Files.size());
        _tracks = new Track[_capacity];

        for (size_t i = 0; i < _capacity; ++i) {
            _tracks[i] = Track(mp3Files[i]);
        }

        _currentIndex = 0;
        return true;
    }
    // Checks if tracks have been loaded into the playlist > 0
    bool HasTracks() const { return _tracks != nullptr && _capacity > 0; }
    size_t GetSize() const { return _capacity; }
    size_t GetCurrentIndex() const { return _currentIndex; }

    const Track* GetCurrentTrack() const {
        if (!HasTracks() || _currentIndex >= _capacity) return nullptr;
        return &_tracks[_currentIndex];
    }
    // Advance to the next track and end if the playlist is complete
    bool NextTrack() {
        if (_currentIndex + 1 < _capacity) {
            _currentIndex++;
            return true;
        }
        return false;
    }
};

// UI handles terminal rendering, input interrogation, and display state.
class PlayerUI {
public:
    void Render(const Track& track, int currentTime, bool isPaused, size_t index, size_t totalTracks) const {
        // Clear the console (Windows specific)
        std::system("cls");
        std::cout << "===============================================================\n";
        std::cout << "               VSC MUSIC PLAYER USER INTERFACE                 \n";
        std::cout << "===============================================================\n\n";
        // Display Track Info (Title, Artist, Status, Progress Bar, Time Left)
        std::cout << " Track [" << (index + 1) << "/" << totalTracks << "]: " << track.GetTitle() << "\n";
        std::cout << " Status:  " << (isPaused ? "[PAUSED]" : "[PLAYING]") << "\n\n";

        // Progress Bar Calculation
        int barWidth = 40;
        float progress = (track.GetDurationSeconds() > 0)
            ? static_cast<float>(currentTime) / track.GetDurationSeconds()
            : 0.0f;
        if (progress > 1.0f) progress = 1.0f;
        int filled = static_cast<int>(barWidth * progress);

        // Display Progress Bar (dynamic to song duration)
        std::cout << " " << Track::FormatTime(currentTime) << " [";
        for (int i = 0; i < barWidth; ++i) {
            if (i < filled) std::cout << "=";
            else if (i == filled) std::cout << ">";
            else std::cout << " ";
        }
        // Total Song Duration at the end of the progress bar
        std::cout << "] " << Track::FormatTime(track.GetDurationSeconds()) << "\n";
        // Song duration remaining at the start of the progress bar
        int remaining = track.GetDurationSeconds() - currentTime;
        if (remaining < 0) remaining = 0;
        std::cout << " Time Left: -" << Track::FormatTime(remaining) << "\n\n";
        std::cout << " Controls: [P] Play/Pause | [S] Skip | [Q] Quit\n";
        std::cout << "===============================================================\n";
        std::cout.flush();
    }
    // Check for non-blocking keyboard input
    bool PollKey(char& outKey) const {
        if (_kbhit()) {
            outKey = static_cast<char>(_getch());
            return true;
        }
        return false;
    }
};

// Music Player manages playback state, ticks, and user input.
class MusicPlayer {
private:
    Playlist _playlist;
    PlayerUI _ui;
    int _elapsedSeconds;
    bool _isPaused;
    bool _isRunning;
    // Main playback loop with non blocking inputs (see comment on line 21)
    void HandleInput() {
        char key;
        if (_ui.PollKey(key)) {
            if (key == 'p' || key == 'P') {
                _isPaused = !_isPaused;
            } else if (key == 's' || key == 'S') {
                AdvanceTrack();
            } else if (key == 'q' || key == 'Q') {
                _isRunning = false;
            }
        }
    }
    // Goes to the next track and resets the time.
    void AdvanceTrack() {
        _elapsedSeconds = 0;
        if (!_playlist.NextTrack()) {
            _isRunning = false;
        }
    }
// Initializes the player and starts the main loop
public:
    MusicPlayer() : _elapsedSeconds(0), _isPaused(false), _isRunning(false) {}
    // Creates a playlist from a directory of mp3 files
    bool Initialize(const std::string& directoryPath) {
        return _playlist.LoadFromDirectory(directoryPath);
    }

    void Run() {
        if (!_playlist.HasTracks()) return;

        _isRunning = true;
        auto lastTick = std::chrono::steady_clock::now();

        while (_isRunning) {
            HandleInput();
            // Update terminal every second
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() >= 1000) {
                lastTick = now;

                const Track* currentTrack = _playlist.GetCurrentTrack();
                if (!currentTrack) break;
                // Update how much time has passed and go to the next track if the current one is finished
                if (!_isPaused) {
                    _elapsedSeconds++;
                    if (_elapsedSeconds > currentTrack->GetDurationSeconds()) {
                        AdvanceTrack();
                        currentTrack = _playlist.GetCurrentTrack();
                        if (!currentTrack) break;
                    }
                }
                // Render the UI with the current track and playback state
                _ui.Render(*currentTrack, _elapsedSeconds, _isPaused, _playlist.GetCurrentIndex(), _playlist.GetSize());
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "\nExiting Music Player\n";
    }
};

int main() {
    // Prompt user for directory path containing MP3 files
    std::string directoryPath;
    std::cout << "Enter directory path containing MP3 files: ";
    std::getline(std::cin, directoryPath);
    // Start the music player with the directory, break if the directory does not have mp3 files or does not exist.
    MusicPlayer player;
    if (!player.Initialize(directoryPath)) {
        std::cerr << "Failed to initialize player. Check directory path and MP3 contents.\n";
        return 1;
    }

    player.Run();
    return 0;
}