// ===========================================
// Enhanced DICOM Segmentation with Distance Map Edge Smoothing + Final Hole Removal
// ===========================================

#include <itkImage.h>
#include <itkImageSeriesReader.h>
#include <itkGDCMImageIO.h>
#include <itkGDCMSeriesFileNames.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkImageFileWriter.h>
#include <itkCurvatureFlowImageFilter.h>
#include <itkBinaryBallStructuringElement.h>
#include <itkGrayscaleDilateImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkConnectedComponentImageFilter.h>
#include <itkRelabelComponentImageFilter.h>
#include <itkBinaryThresholdImageFilter.h>
#include <itkVotingBinaryHoleFillingImageFilter.h>
#include <itkBinaryFillholeImageFilter.h>

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>

using FloatImageType = itk::Image<float, 3>;
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

MaskType::Pointer SegmentLiver(FloatImageType::Pointer image, 
                                const FloatImageType::IndexType& seed,
                                int lower, int upper) {
    auto t = std::time(nullptr);
    std::stringstream ts;
    ts << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    std::string timestamp = ts.str();

    using WriterType = itk::ImageFileWriter<MaskType>;
    std::cout << "Seed intensity = " << image->GetPixel(seed) << std::endl;

    // Step 1: Region growing
    using FilterType = itk::ConnectedThresholdImageFilter<FloatImageType, MaskType>;
    FilterType::Pointer seg = FilterType::New();
    seg->SetInput(image);
    seg->AddSeed(seed);
    seg->SetLower(lower);
    seg->SetUpper(upper);
    seg->SetReplaceValue(255);
    seg->Update();

    MaskType::Pointer mask = seg->GetOutput();

    WriterType::Pointer writerA = WriterType::New();
    writerA->SetFileName("../dataset/result/debug_mask_raw_" + timestamp + ".nii.gz");
    writerA->SetInput(mask);
    writerA->Update();

    // Step 2: Dilation
    using StructuringElementType = itk::BinaryBallStructuringElement<unsigned char, 3>;
    StructuringElementType element;
    element.SetRadius(2);
    element.CreateStructuringElement();

    using DilateType = itk::GrayscaleDilateImageFilter<MaskType, MaskType, StructuringElementType>;
    DilateType::Pointer dilate = DilateType::New();
    dilate->SetInput(mask);
    dilate->SetKernel(element);
    dilate->Update();

    // Step 3: Hole filling
    using HoleFillingType = itk::VotingBinaryHoleFillingImageFilter<MaskType, MaskType>;
    HoleFillingType::Pointer holeFiller = HoleFillingType::New();
    holeFiller->SetInput(dilate->GetOutput());
    MaskType::SizeType radius = {{4, 4, 2}};
    holeFiller->SetRadius(radius);
    holeFiller->SetMajorityThreshold(2);
    holeFiller->SetBackgroundValue(0);
    holeFiller->SetForegroundValue(255);
    holeFiller->Update();

    WriterType::Pointer writerC = WriterType::New();
    writerC->SetFileName("../dataset/result/debug_mask_filled_" + timestamp + ".nii.gz");
    writerC->SetInput(holeFiller->GetOutput());
    writerC->Update();

    // Step 4: Keep largest connected component
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

    WriterType::Pointer writerD = WriterType::New();
    writerD->SetFileName("../dataset/result/debug_mask_largest_" + timestamp + ".nii.gz");
    writerD->SetInput(threshold->GetOutput());
    writerD->Update();

    // Step 5: Curvature-based smoothing
    using FloatMask = itk::Image<float, 3>;
    using CastToFloat = itk::CastImageFilter<MaskType, FloatMask>;
    using CastBack = itk::CastImageFilter<FloatMask, MaskType>;

    CastToFloat::Pointer toFloat = CastToFloat::New();
    toFloat->SetInput(threshold->GetOutput());

    using Smoother = itk::CurvatureFlowImageFilter<FloatMask, FloatMask>;
    Smoother::Pointer smooth = Smoother::New();
    smooth->SetInput(toFloat->GetOutput());
    smooth->SetNumberOfIterations(5);
    smooth->SetTimeStep(0.125);

    CastBack::Pointer backToUChar = CastBack::New();
    backToUChar->SetInput(smooth->GetOutput());
    backToUChar->Update();

    // Step 6: Final hole fill to eliminate small voids introduced by smoothing
    using FinalFillType = itk::BinaryFillholeImageFilter<MaskType>;
    FinalFillType::Pointer fillHoles = FinalFillType::New();
    fillHoles->SetInput(backToUChar->GetOutput());
    fillHoles->SetForegroundValue(255);
    fillHoles->Update();

    WriterType::Pointer writerE = WriterType::New();
    writerE->SetFileName("../dataset/result/debug_mask_final_" + timestamp + ".nii.gz");
    writerE->SetInput(fillHoles->GetOutput());
    writerE->Update();

    return fillHoles->GetOutput();
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

    auto finalMask = SegmentLiver(image, seed, lower, upper);
    if (!finalMask) return EXIT_FAILURE;

    std::filesystem::create_directories("../dataset/result/");
    auto t = std::time(nullptr);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();

    std::string imageFilename = "../dataset/result/image_" + timestamp + ".nii.gz";
    std::string maskFilename = "../dataset/result/mask_" + timestamp + ".nii.gz";

    using ImageWriterType = itk::ImageFileWriter<FloatImageType>;
    ImageWriterType::Pointer writer1 = ImageWriterType::New();
    writer1->SetFileName(imageFilename);
    writer1->SetInput(image);
    writer1->Update();

    using MaskWriterType = itk::ImageFileWriter<MaskType>;
    MaskWriterType::Pointer writer2 = MaskWriterType::New();
    writer2->SetFileName(maskFilename);
    writer2->SetInput(finalMask);
    writer2->Update();

    std::cout << "Saved " << imageFilename << " and " << maskFilename << " to ../dataset/result/" << std::endl;
    return EXIT_SUCCESS;
}




// ----------------------older version---------------------------------
// ===========================================
// Enhanced DICOM Segmentation with Dilation + Hole Filling Strategy
// ===========================================

// #include <itkImage.h>
// #include <itkImageSeriesReader.h>
// #include <itkGDCMImageIO.h>
// #include <itkGDCMSeriesFileNames.h>
// #include <itkConnectedThresholdImageFilter.h>
// #include <itkImageFileWriter.h>
// #include <itkCurvatureFlowImageFilter.h>
// #include <itkBinaryBallStructuringElement.h>
// #include <itkGrayscaleDilateImageFilter.h>
// #include <itkCastImageFilter.h>
// #include <itkConnectedComponentImageFilter.h>
// #include <itkRelabelComponentImageFilter.h>
// #include <itkBinaryThresholdImageFilter.h>
// #include <itkVotingBinaryHoleFillingImageFilter.h>

// #include <iostream>
// #include <string>
// #include <sstream>
// #include <iomanip>
// #include <ctime>
// #include <filesystem>

// using FloatImageType = itk::Image<float, 3>;
// using MaskType = itk::Image<unsigned char, 3>;

// FloatImageType::Pointer ReadDICOMSeries(const std::string& dir) {
//     using NamesGeneratorType = itk::GDCMSeriesFileNames;
//     using ReaderType = itk::ImageSeriesReader<FloatImageType>;

//     NamesGeneratorType::Pointer nameGen = NamesGeneratorType::New();
//     nameGen->SetDirectory(dir);
//     const auto uids = nameGen->GetSeriesUIDs();
//     if (uids.empty()) {
//         std::cerr << "No DICOM series found in " << dir << std::endl;
//         return nullptr;
//     }

//     ReaderType::Pointer reader = ReaderType::New();
//     reader->SetImageIO(itk::GDCMImageIO::New());
//     reader->SetFileNames(nameGen->GetFileNames(uids[0]));

//     try {
//         reader->Update();
//     } catch (itk::ExceptionObject& e) {
//         std::cerr << "DICOM Read Error: " << e << std::endl;
//         return nullptr;
//     }

//     return reader->GetOutput();
// }

// MaskType::Pointer SegmentLiver(FloatImageType::Pointer image, 
//                                 const FloatImageType::IndexType& seed,
//                                 int lower, int upper) {
//     auto t = std::time(nullptr);
//     std::stringstream ts;
//     ts << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
//     std::string timestamp = ts.str();

//     using WriterType = itk::ImageFileWriter<MaskType>;
//     std::cout << "Seed intensity = " << image->GetPixel(seed) << std::endl;

//     // Step 1: Threshold-based region growing
//     using FilterType = itk::ConnectedThresholdImageFilter<FloatImageType, MaskType>;
//     FilterType::Pointer seg = FilterType::New();
//     seg->SetInput(image);
//     seg->AddSeed(seed);
//     seg->SetLower(lower);
//     seg->SetUpper(upper);
//     seg->SetReplaceValue(255);
//     seg->Update();

//     MaskType::Pointer mask = seg->GetOutput();

//     WriterType::Pointer writerA = WriterType::New();
//     writerA->SetFileName("../dataset/result/debug_mask_raw_" + timestamp + ".nii.gz");
//     writerA->SetInput(mask);
//     writerA->Update();

//     // Step 2: Morphological Dilation
//     using StructuringElementType = itk::BinaryBallStructuringElement<unsigned char, 3>;
//     StructuringElementType element;
//     element.SetRadius(2);
//     element.CreateStructuringElement();

//     using DilateType = itk::GrayscaleDilateImageFilter<MaskType, MaskType, StructuringElementType>;
//     DilateType::Pointer dilate = DilateType::New();
//     dilate->SetInput(mask);
//     dilate->SetKernel(element);
//     dilate->Update();

//     // Step 3: Hole filling
//     using HoleFillingType = itk::VotingBinaryHoleFillingImageFilter<MaskType, MaskType>;
//     HoleFillingType::Pointer holeFiller = HoleFillingType::New();
//     holeFiller->SetInput(dilate->GetOutput());
//     MaskType::SizeType radius = {{4, 4, 2}};
//     holeFiller->SetRadius(radius);
//     holeFiller->SetMajorityThreshold(2);
//     holeFiller->SetBackgroundValue(0);
//     holeFiller->SetForegroundValue(255);
//     holeFiller->Update();

//     WriterType::Pointer writerC = WriterType::New();
//     writerC->SetFileName("../dataset/result/debug_mask_filled_" + timestamp + ".nii.gz");
//     writerC->SetInput(holeFiller->GetOutput());
//     writerC->Update();

//     // Step 4: Keep largest connected component
//     using LabelType = itk::Image<unsigned int, 3>;
//     using ConnectedComponentType = itk::ConnectedComponentImageFilter<MaskType, LabelType>;
//     using RelabelType = itk::RelabelComponentImageFilter<LabelType, LabelType>;
//     using ThresholdType = itk::BinaryThresholdImageFilter<LabelType, MaskType>;

//     ConnectedComponentType::Pointer cc = ConnectedComponentType::New();
//     cc->SetInput(holeFiller->GetOutput());

//     RelabelType::Pointer relabel = RelabelType::New();
//     relabel->SetInput(cc->GetOutput());

//     ThresholdType::Pointer threshold = ThresholdType::New();
//     threshold->SetInput(relabel->GetOutput());
//     threshold->SetLowerThreshold(1);
//     threshold->SetUpperThreshold(1);
//     threshold->SetInsideValue(255);
//     threshold->SetOutsideValue(0);
//     threshold->Update();

//     WriterType::Pointer writerD = WriterType::New();
//     writerD->SetFileName("../dataset/result/debug_mask_largest_" + timestamp + ".nii.gz");
//     writerD->SetInput(threshold->GetOutput());
//     writerD->Update();

//     // Step 5: Optional smoothing of final mask
//     using FloatMask = itk::Image<float, 3>;
//     using CastToFloat = itk::CastImageFilter<MaskType, FloatMask>;
//     using CastBack = itk::CastImageFilter<FloatMask, MaskType>;
    
//     CastToFloat::Pointer toFloat = CastToFloat::New();
//     toFloat->SetInput(threshold->GetOutput());

//     using Smoother = itk::CurvatureFlowImageFilter<FloatMask, FloatMask>;
//     Smoother::Pointer smooth = Smoother::New();
//     smooth->SetInput(toFloat->GetOutput());
//     smooth->SetNumberOfIterations(5);
//     smooth->SetTimeStep(0.125);

//     CastBack::Pointer backToUChar = CastBack::New();
//     backToUChar->SetInput(smooth->GetOutput());
//     backToUChar->Update();

//     WriterType::Pointer writerE = WriterType::New();
//     writerE->SetFileName("../dataset/result/debug_mask_smoothed_" + timestamp + ".nii.gz");
//     writerE->SetInput(backToUChar->GetOutput());
//     writerE->Update();

//     return backToUChar->GetOutput();
// }

// int main(int argc, char* argv[]) {
//     if (argc < 7) {
//         std::cerr << "Usage: " << argv[0] << " <dicom_dir> <seed_x> <seed_y> <seed_z> <lower> <upper>" << std::endl;
//         return EXIT_FAILURE;
//     }

//     const std::string dicomDir = argv[1];
//     FloatImageType::IndexType seed;
//     seed[0] = std::stoi(argv[2]);
//     seed[1] = std::stoi(argv[3]);
//     seed[2] = std::stoi(argv[4]);
//     int lower = std::stoi(argv[5]);
//     int upper = std::stoi(argv[6]);

//     auto image = ReadDICOMSeries(dicomDir);
//     if (!image) return EXIT_FAILURE;

//     auto mask = SegmentLiver(image, seed, lower, upper);
//     if (!mask) return EXIT_FAILURE;

//     std::filesystem::create_directories("../dataset/result/");
//     auto t = std::time(nullptr);
//     std::stringstream ss;
//     ss << std::put_time(std::localtime(&t), "%Y%m%d_%H%M%S");
//     std::string timestamp = ss.str();

//     std::string imageFilename = "../dataset/result/image_" + timestamp + ".nii.gz";
//     std::string maskFilename = "../dataset/result/mask_" + timestamp + ".nii.gz";

//     using ImageWriterType = itk::ImageFileWriter<FloatImageType>;
//     ImageWriterType::Pointer writer1 = ImageWriterType::New();
//     writer1->SetFileName(imageFilename);
//     writer1->SetInput(image);
//     writer1->Update();

//     using MaskWriterType = itk::ImageFileWriter<MaskType>;
//     MaskWriterType::Pointer writer2 = MaskWriterType::New();
//     writer2->SetFileName(maskFilename);
//     writer2->SetInput(mask);
//     writer2->Update();

//     std::cout << "Saved " << imageFilename << " and " << maskFilename << " to ../dataset/result/" << std::endl;
//     return EXIT_SUCCESS;
// }

