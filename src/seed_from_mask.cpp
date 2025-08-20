#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkImageRegionConstIterator.h>


#include <vector>
#include <numeric>
#include <iostream>

using MaskType = itk::Image<unsigned char, 3>;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <mask.nii.gz>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string maskPath = argv[1];
    using ReaderType = itk::ImageFileReader<MaskType>;
    auto reader = ReaderType::New();
    reader->SetFileName(maskPath);

    try {
        reader->Update();
    } catch (itk::ExceptionObject& err) {
        std::cerr << "Error reading mask: " << err << std::endl;
        return EXIT_FAILURE;
    }

    auto mask = reader->GetOutput();
    itk::ImageRegionConstIterator<MaskType> it(mask, mask->GetLargestPossibleRegion());

    std::vector<long> xList, yList, zList;

    for (it.GoToBegin(); !it.IsAtEnd(); ++it) {
        if (it.Get() > 0) {
            auto idx = it.GetIndex();
            xList.push_back(idx[0]);
            yList.push_back(idx[1]);
            zList.push_back(idx[2]);
        }
    }

    if (xList.empty()) {
        std::cerr << "No foreground pixels found in mask." << std::endl;
        return EXIT_FAILURE;
    }

    long meanX = std::accumulate(xList.begin(), xList.end(), 0L) / xList.size();
    long meanY = std::accumulate(yList.begin(), yList.end(), 0L) / yList.size();
    long meanZ = std::accumulate(zList.begin(), zList.end(), 0L) / zList.size();

    std::cout << "Suggested seed index: " << meanX << " " << meanY << " " << meanZ << std::endl;

    return EXIT_SUCCESS;
}
