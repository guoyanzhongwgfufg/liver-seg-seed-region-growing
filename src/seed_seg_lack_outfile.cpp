// ===========================================
// Enhanced DICOM Segmentation with Pre/Post-processing
// Includes: Opening, Hole Filling, Largest Component Filtering, Timestamped Output
// ===========================================

#include <itkImage.h>
#include <itkImageSeriesReader.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkCurvatureFlowImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkBinaryMorphologicalClosingImageFilter.h>
#include <itkBinaryMorphologicalOpeningImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkVotingBinaryHoleFillingImageFilter.h>

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>

using FloatImageType = itk::Image<float, 3>;
using ShortImageType = itk::Image<short, 3>;
using MaskType = itk::Image<unsigned char, 3>;

FloatImageType::Pointer ReadDICOMSeries(const std::string& dir) {
    using NamesGeneratorType = itk::GDCMSeriesFileNames;
    using ReaderType = itk::ImageSeriesReader<FloatImageType>;

    NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();
    nameGen->SetDirectory(dir);
    const auto uids = nameGen->GetSeriesUIDs();
    if (uids.empty()) {
        std::cerr << "No DICOM series found in " << dir << std::endl;
        return nullptr;
    }

    ReaderType::Pointer reader = ReaderType::New();
    reader->SetImageIO(itk::GDCMImageIO::New());
    reader->SetFileNames(nameGen->GetFileNames(uids[0]));

    try {
        reader->Update();
    } catch (itk::ExceptionObject& e) {
        std::cerr << "DICOM Read Error: " << e << std::endl;
        return nullptr;
    }

    return reader->GetOutput();
}
std::cout << "Seed intensity = " << image->GetPixel(seed) << std::endl;

MaskType::Pointer SegmentLiver(FloatImageType::Pointer image, 
                                const FloatImageType::IndexType& seed,
                                int lower, int upper) {
    using SmootherType = itk::CurvatureFlowImageFilter<FloatImageType, FloatImageType>;
    SmootherType::Pointer smoother = SmootherType::New();
    FloatImageType::Pointer smoothedImage = image;  // 不平滑


    using FilterType = itk::ConnectedThresholdImageFilter<FloatImageType, MaskType>;
    FilterType::Pointer seg = FilterType::New();
    seg->SetInput(smoothedImage);
    seg->AddSeed(seed);
    seg->SetLower(lower);
    seg->SetUpper(upper);
    seg->SetReplaceValue(255);
    seg->Update();

    MaskType::Pointer mask = seg->GetOutput();

    // Opening
    using StructuringElementType = itk::BinaryBallStructuringElement<unsigned char, 3>;
    StructuringElementType element;
    element.SetRadius(2);
    element.CreateStructuringElement();

    using OpeningFilterType = itk::BinaryMorphologicalOpeningImageFilter<MaskType, MaskType, StructuringElementType>;
    OpeningFilterType::Pointer opening = OpeningFilterType::New();
    opening->SetInput(mask);
    opening->SetKernel(element);
    opening->SetForegroundValue(255);
    opening->Update();

    // Closing
    using ClosingFilterType = itk::BinaryMorphologicalClosingImageFilter<MaskType, MaskType, StructuringElementType>;
    ClosingFilterType::Pointer closing = ClosingFilterType::New();
    closing->SetInput(opening->GetOutput());
    closing->SetKernel(element);
    closing->SetForegroundValue(255);
    closing->Update();

    // Hole filling
    using HoleFillingType = itk::VotingBinaryHoleFillingImageFilter<MaskType, MaskType>;
    HoleFillingType::Pointer holeFiller = HoleFillingType::New();
    holeFiller->SetInput(closing->GetOutput());
    MaskType::SizeType radius = {{2, 2, 2}};
    holeFiller->SetRadius(radius);
    holeFiller->SetMajorityThreshold(1);
    holeFiller->SetBackgroundValue(0);
    holeFiller->SetForegroundValue(255);
    holeFiller->Update();

    // Keep largest component
    using LabelType = itk::Image<unsigned int, 3>;
    using ConnectedComponentType = itk::ConnectedComponentImageFilter<MaskType, LabelType>;
    using RelabelType = itk::RelabelComponentImageFilter<LabelType, LabelType>;
    using ThresholdType = itk::BinaryThresholdImageFilter<LabelType, MaskType>;

    ConnectedComponentType::Pointer cc = ConnectedComponentType::New();
    cc->SetInput(holeFiller->GetOutput());

    RelabelType::Pointer relabel = RelabelType::New();
    relabel->SetInput(cc->GetOutput());

    ThresholdType::Pointer threshold = ThresholdType::New();
    threshold->SetInput(relabel->GetOutput());
    threshold->SetLowerThreshold(1);
    threshold->SetUpperThreshold(1);
    threshold->SetInsideValue(255);
    threshold->SetOutsideValue(0);
    threshold->Update();

    return threshold->GetOutput();
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        std::cerr << "Usage: " << argv[0] << " <dicom_dir> <seed_x> <seed_y> <seed_z> <lower> <upper>" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string dicomDir = argv[1];
    FloatImageType::IndexType seed;
    seed[0] = std::stoi(argv[2]);
    seed[1] = std::stoi(argv[3]);
    seed[2] = std::stoi(argv[4]);
    int lower = std::stoi(argv[5]);
    int upper = std::stoi(argv[6]);

    auto image = ReadDICOMSeries(dicomDir);
    if (!image) return EXIT_FAILURE;

    auto mask = SegmentLiver(image, seed, lower, upper);
    if (!mask) return EXIT_FAILURE;

    std::filesystem::create_directories("../dataset/result/");
    auto t = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();

    std::string imageFilename = "../dataset/result/image_" + timestamp + ".nii.gz";
    std::string maskFilename = "../dataset/result/mask_" + timestamp + ".nii.gz";

    using WriterType = itk::ImageFileWriter<FloatImageType>;
    WriterType::Pointer writer1 = WriterType::New();
    writer1->SetFileName(imageFilename);
    writer1->SetInput(image);
    writer1->Update();

    using MaskWriterType = itk::ImageFileWriter<MaskType>;
    MaskWriterType::Pointer writer2 = MaskWriterType::New();
    writer2->SetFileName(maskFilename);
    writer2->SetInput(mask);
    writer2->Update();

    std::cout << "Saved " << imageFilename << " and " << maskFilename << " to ../dataset/result/" << std::endl;
    return EXIT_SUCCESS;
}
