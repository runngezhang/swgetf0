
#include <algorithm>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cstring>
#include <cassert>

#include "GetF0/get_f0.h"


namespace {

template <class SourceFormat, class DestFormat>
struct StaticCaster {
  DestFormat operator()(SourceFormat from)
  {
    return static_cast<DestFormat>(from);
  }
};

template <class SourceFormat, class DestFormat>
int readFile(const std::string& fileName, std::vector<DestFormat>& output)
{
  std::FILE* inputFile = std::fopen(fileName.c_str(), "rb");
  if (inputFile == nullptr) {
    std::perror("Cannot open input");
    return 1;
  }

  if (std::fseek(inputFile, 0, SEEK_END) == -1) {
    std::perror("Cannot seek to end");
    return 1;
  }

  auto endPosition = std::ftell(inputFile);
  if (endPosition == -1) {
    std::perror("Cannot get end position");
    return 1;
  }

  if (std::fseek(inputFile, 0, SEEK_SET) == -1) {
    std::perror("Cannot seek to beginning");
    return 1;
  }

  if (endPosition % sizeof(SourceFormat) != 0) {
    std::cerr << "Assertion failed: File size not a multiple of format type"
              << std::endl;
    return 1;
  }

  auto numberSamples = endPosition / sizeof(SourceFormat);

  output.reserve(numberSamples);

  enum { BUFFER_SIZE = 4096 };
  SourceFormat buffer[BUFFER_SIZE];
  size_t readSize;

  do {
    readSize = std::fread(buffer, sizeof(SourceFormat), BUFFER_SIZE, inputFile);

    std::transform(buffer, buffer + readSize, std::back_inserter(output),
                   StaticCaster<SourceFormat, DestFormat>());

  } while (readSize == BUFFER_SIZE);

  if (std::ferror(inputFile) != 0) {
    std::perror("Error reading file");
    return 1;
  }

  if (std::fclose(inputFile) != 0) {
    std::perror("Error closing file");
    return 1;
  }

  return 0;
}



class GetF0_impl : public GetF0::GetF0 {
public:

  typedef std::vector<Sample> SampleVector;
  typedef std::vector<float> OutputVector;

  GetF0_impl(SampleFrequency sampleFrequency,
	     SampleVector& samples);

protected:

  long read_samples(float** buffer, long num_records) override;

  long read_samples_overlap(float** buffer, long num_records,
                            long step) override;

  void write_output(float* f0p, float* vuvp, float* rms_speech, float* acpkp,
                    int vecsize) override;

private:

  SampleVector& m_samples;

  unsigned m_position;

public:
  OutputVector m_outputVector;
};

GetF0_impl::GetF0_impl(SampleFrequency sampleFrequency, SampleVector& samples)
    : GetF0(sampleFrequency), m_samples(samples), m_position(0)
{
}

long GetF0_impl::read_samples(float** buffer, long num_records) {
  auto range = std::min<long>(
      num_records, m_samples.size() - m_position);
  *buffer = &m_samples[m_position];
  m_position += range;
  return range;
}

long GetF0_impl::read_samples_overlap(float** buffer, long num_records,
                                      long step)
{
  // (num_records - step) old records
  // (step) new records

  m_position -= (num_records - step);
  return read_samples(buffer, num_records);
}

void GetF0_impl::write_output(float* f0p, float* vuvp, float* rms_speech,
                              float* acpkp, int vecsize)
{
  std::reverse_copy(f0p, f0p + vecsize, std::back_inserter(m_outputVector));
}

} // namespace anonymous



int main(int argc, char* argv[])
{
  typedef short DiskSample;

  GetF0_impl::SampleVector samples;
  if (readFile<DiskSample, GetF0_impl::Sample>(argv[1], samples) != 0) {
    return 1;
  }

  std::cout << "Read " << samples.size() << " samples." << std::endl;


  GetF0_impl::SampleFrequency freq = 16000;
  GetF0_impl f0(freq, samples);
  f0.init();
  f0.run();

  std::cout << "Returned " << f0.m_outputVector.size() << " data points."
            << std::endl;
  std::cout << std::endl;

  for (int i = 0; i < f0.m_outputVector.size(); ++i) {
    if (i > 0 && i % 10 == 0) {
      std::cout << std::endl;
    }

    std::cout << f0.m_outputVector[i] << " ";
  }
  std::cout << std::endl;

  return 0;
}
