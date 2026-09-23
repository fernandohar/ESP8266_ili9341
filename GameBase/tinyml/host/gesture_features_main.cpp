// Host-side driver for the firmware's own gesture feature extractor.
//
// Reads the raw capture CSV (one row per sampled point) and writes one row per
// episode: label followed by GESTURE_FEATURE_COUNT features. The point of routing
// the PC pipeline through this instead of a Python re-implementation is that there
// is then exactly one extractor, so training features and on-device features
// cannot drift apart.
//
// Built automatically by tinyml/prepare_gestures.py; to do it by hand:
//   c++ -std=c++11 -O2 -I../../src tinyml/host/gesture_features_main.cpp \
//       -o tinyml/host/gesture_features
//
// Usage: gesture_features <raw.csv> [more.csv ...]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "ml/GestureEpisode.h"
#include "ml/GestureFeatures.h"

namespace {

struct EpisodeKey {
  std::string boot;
  long episode;

  bool operator<(const EpisodeKey &other) const {
    if (boot != other.boot) return boot < other.boot;
    return episode < other.episode;
  }
};

struct Episode {
  std::string label;
  uint8_t labelIndex;
  std::vector<GestureSample> samples;
};

uint8_t strokeCount(const std::vector<GestureSample> &samples) {
  if (samples.empty()) {
    return 0;
  }
  return (uint8_t)(samples.back().stroke + 1);
}

std::vector<std::string> splitCsv(const std::string &line) {
  std::vector<std::string> fields;
  std::string current;
  for (size_t i = 0; i < line.size(); i++) {
    const char c = line[i];
    if (c == ',') {
      fields.push_back(current);
      current.clear();
    } else if (c != '\r' && c != '\n') {
      current.push_back(c);
    }
  }
  fields.push_back(current);
  return fields;
}

bool labelIndex(const std::string &name, uint8_t *index) {
  for (uint8_t i = 0; i < GESTURE_LABEL_COUNT; i++) {
    if (name == gestureLabelName(i)) {
      *index = i;
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s <raw.csv> [more.csv ...]\n", argv[0]);
    return 2;
  }

  std::map<EpisodeKey, Episode> episodes;
  std::set<EpisodeKey> discarded;
  // Insertion order, so the output follows the order gestures were performed and
  // a train/test split by row is not secretly ordered by class.
  std::vector<EpisodeKey> order;

  for (int arg = 1; arg < argc; arg++) {
    FILE *file = fopen(argv[arg], "r");
    if (file == NULL) {
      fprintf(stderr, "cannot open %s\n", argv[arg]);
      return 1;
    }

    char buffer[512];
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
      std::vector<std::string> fields = splitCsv(std::string(buffer));
      if (fields.size() < 8 || fields[0] != "MLG") {
        continue;
      }
      // Appending capture sessions to one file repeats the header row.
      if (fields[1] == "boot") {
        continue;
      }

      EpisodeKey key;
      key.boot = fields[1];
      key.episode = strtol(fields[2].c_str(), NULL, 10);

      if (fields[3] == "DISCARD") {
        discarded.insert(key);
        continue;
      }

      uint8_t label = 0;
      if (!labelIndex(fields[3], &label)) {
        continue;
      }

      GestureSample sample;
      sample.stroke = (uint8_t)strtol(fields[4].c_str(), NULL, 10);
      sample.t = (uint16_t)strtol(fields[5].c_str(), NULL, 10);
      sample.x = (int16_t)strtol(fields[6].c_str(), NULL, 10);
      sample.y = (int16_t)strtol(fields[7].c_str(), NULL, 10);

      if (episodes.find(key) == episodes.end()) {
        order.push_back(key);
        episodes[key].label = fields[3];
        episodes[key].labelIndex = label;
      }
      Episode &episode = episodes[key];
      if (episode.samples.size() < GESTURE_MAX_SAMPLES) {
        episode.samples.push_back(sample);
      }
    }
    fclose(file);
  }

  // boot and episode ride along so training can hold out whole capture sessions.
  // Episodes recorded back to back in one sitting are near-duplicates, so a random
  // split would report an accuracy the device will not reproduce.
  printf("boot,episode,label");
  for (int i = 0; i < GESTURE_FEATURE_COUNT; i++) {
    printf(",%s", gestureFeatureName(i));
  }
  printf("\n");

  long written = 0;
  long skipped = 0;
  std::map<std::string, long> malformed;
  for (size_t i = 0; i < order.size(); i++) {
    const EpisodeKey &key = order[i];
    if (discarded.count(key) > 0) {
      continue;
    }
    const Episode &episode = episodes[key];
    if (episode.samples.empty()) {
      continue;
    }

    // Same structural rule the capture screen now enforces, applied to captures
    // recorded before it existed. Dropped loudly rather than quietly: these rows
    // carry a label the gesture cannot have had.
    if (strokeCount(episode.samples) < gestureLabelMinStrokes(episode.labelIndex)) {
      malformed[episode.label]++;
      continue;
    }

    float features[GESTURE_FEATURE_COUNT];
    if (!gestureExtractFeatures(episode.samples.data(),
                               (uint8_t)episode.samples.size(), features)) {
      skipped++;
      continue;
    }

    printf("%s,%ld,%s", key.boot.c_str(), key.episode, episode.label.c_str());
    for (int f = 0; f < GESTURE_FEATURE_COUNT; f++) {
      printf(",%.5f", features[f]);
    }
    printf("\n");
    written++;
  }

  fprintf(stderr, "%ld episodes -> features, %lu discarded on device, %ld unusable\n",
          written, (unsigned long)discarded.size(), skipped);
  for (std::map<std::string, long>::const_iterator it = malformed.begin();
       it != malformed.end(); ++it) {
    fprintf(stderr,
            "dropped %ld %s episodes with too few strokes to be that gesture\n",
            it->second, it->first.c_str());
  }
  return 0;
}
