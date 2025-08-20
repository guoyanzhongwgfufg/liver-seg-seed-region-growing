// =======================================================
// Liver & Lung Segmentation - 三视图 + 掩码显示 + 自动分割 + 交互选种子
// =======================================================

#include <vtkSmartPointer.h>
#include <vtkDICOMImageReader.h>
#include <vtkImageViewer2.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkImageMapToWindowLevelColors.h>
#include <vtkImageActor.h>
#include <vtkInteractorStyleImage.h>
#include <vtkCommand.h>
#include <vtkPointPicker.h>

#include <itkImage.h>
#include <itkImageFileReader.h>
#include <itkConnectedThresholdImageFilter.h>
#include <itkCastImageFilter.h>
#include <itkVTKImageToImageFilter.h>
#include <itkImageToVTKImageFilter.h>

#include <iostream>
#include <string>
#include <array>
#include <vector>

using ImageType = itk::Image<short, 3>;
using MaskType = itk::Image<unsigned char, 3>;

std::vector<ImageType::IndexType> g_seedPoints;

class MouseInteractorStyle : public vtkInteractorStyleImage {
public:
    static MouseInteractorStyle* New();
    vtkTypeMacro(MouseInteractorStyle, vtkInteractorStyleImage);

    void SetImage(ImageType::Pointer img) { image = img; }

    virtual void OnLeftButtonDown() override {
        int* clickPos = this->GetInteractor()->GetEventPosition();

        vtkSmartPointer<vtkPointPicker> picker = vtkSmartPointer<vtkPointPicker>::New();
        picker->Pick(clickPos[0], clickPos[1], 0, this->GetDefaultRenderer());
        double picked[3];
        picker->GetPickPosition(picked);

        ImageType::IndexType index;
        ImageType::PointType point;
        point[0] = picked[0]; point[1] = picked[1]; point[2] = picked[2];
        image->TransformPhysicalPointToIndex(point, index);

        std::cout << "添加种子点: " << index << std::endl;
        g_seedPoints.push_back(index);

        vtkInteractorStyleImage::OnLeftButtonDown();
    }

private:
    ImageType::Pointer image;
};

vtkStandardNewMacro(MouseInteractorStyle);

MaskType::Pointer AutoSegment(ImageType::Pointer image) {
    using FilterType = itk::ConnectedThresholdImageFilter<ImageType, MaskType>;
    FilterType::Pointer filter = FilterType::New();
    filter->SetInput(image);

    for (const auto& seed : g_seedPoints) {
        filter->AddSeed(seed);
    }

    filter->SetLower(30);
    filter->SetUpper(150);
    filter->SetReplaceValue(255);

    try {
        filter->Update();
    } catch (itk::ExceptionObject& err) {
        std::cerr << "分割失败: " << err << std::endl;
        return nullptr;
    }

    return filter->GetOutput();
}

void DisplayThreeViews(ImageType::Pointer itkImage, MaskType::Pointer maskImage) {
    using ConnectorType = itk::ImageToVTKImageFilter<ImageType>;
    ConnectorType::Pointer connector = ConnectorType::New();
    connector->SetInput(itkImage);
    connector->Update();

    auto vtkImage = connector->GetOutput();

    const std::array<std::string, 3> orientations = {"Axial", "Coronal", "Sagittal"};

    for (int i = 0; i < 3; ++i) {
        vtkSmartPointer<vtkImageViewer2> viewer = vtkSmartPointer<vtkImageViewer2>::New();
        viewer->SetInputData(vtkImage);
        viewer->SetSliceOrientation(i);
        viewer->SetSlice(viewer->GetSliceMax() / 2);

        vtkSmartPointer<vtkRenderWindowInteractor> iren = vtkSmartPointer<vtkRenderWindowInteractor>::New();
        vtkSmartPointer<MouseInteractorStyle> style = vtkSmartPointer<MouseInteractorStyle>::New();
        style->SetImage(itkImage);

        viewer->SetupInteractor(iren);
        iren->SetInteractorStyle(style);

        viewer->Render();
        viewer->GetRenderer()->SetBackground(0.1, 0.1, 0.1);
        viewer->GetRenderWindow()->SetWindowName(orientations[i].c_str());

        iren->Start();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <dicom_dir> [<mask_file.nii.gz>]" << std::endl;
        return EXIT_FAILURE;
    }

    const std::string dicomDir = argv[1];
    const bool hasMask = (argc >= 3);
    const std::string maskFile = hasMask ? argv[2] : "";

    auto reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetDirectoryName(dicomDir.c_str());
    reader->Update();

    using ImportFilterType = itk::VTKImageToImageFilter<ImageType>;
    ImportFilterType::Pointer vtkToItk = ImportFilterType::New();
    vtkToItk->SetInput(reader->GetOutput());
    vtkToItk->Update();
    ImageType::Pointer image = vtkToItk->GetOutput();

    MaskType::Pointer mask;
    if (hasMask) {
        using MaskReaderType = itk::ImageFileReader<MaskType>;
        MaskReaderType::Pointer maskReader = MaskReaderType::New();
        maskReader->SetFileName(maskFile);
        try {
            maskReader->Update();
            mask = maskReader->GetOutput();
        } catch (itk::ExceptionObject& err) {
            std::cerr << "读取掩码失败: " << err << std::endl;
            return EXIT_FAILURE;
        }
    } else {
        std::cout << "未提供掩码，请在窗口中点击选择种子点，关闭后将开始分割..." << std::endl;
        DisplayThreeViews(image, nullptr);
        mask = AutoSegment(image);
        if (!mask) return EXIT_FAILURE;
    }

    DisplayThreeViews(image, mask);

    return EXIT_SUCCESS;
}




// #-----------order visualization version---------------------------
// #include <itkImage.h>
// #include <itkImageSeriesReader.h>
// #include <itkImageFileReader.h>
// #include <itkGDCMImageIO.h>
// #include <itkGDCMSeriesFileNames.h>
// #include <itkRescaleIntensityImageFilter.h>
// #include <itkCastImageFilter.h>
// #include <itkLabelOverlayImageFilter.h>
// #include <itkImageToVTKImageFilter.h>

// #include <vtkSmartPointer.h>
// #include <vtkRenderWindowInteractor.h>
// #include <vtkRenderer.h>
// #include <vtkRenderWindow.h>
// #include <vtkImageViewer2.h>
// #include <vtkImageActor.h>
// #include <vtkInteractorStyleImage.h>
// #include <vtkCommand.h>
// #include <vtkImageMapToWindowLevelColors.h>

// using PixelType = short;
// constexpr unsigned int Dimension = 3;
// using ImageType = itk::Image<PixelType, Dimension>;
// using RGBImageType = itk::Image<itk::RGBPixel<unsigned char>, Dimension>;
// using MaskType = itk::Image<unsigned char, Dimension>;

// ImageType::Pointer LoadDICOMSeries(const std::string& dir)
// {
//     using ReaderType = itk::ImageSeriesReader<ImageType>;
//     auto reader = ReaderType::New();
//     auto dicomIO = itk::GDCMImageIO::New();
//     auto nameGen = itk::GDCMSeriesFileNames::New();

//     nameGen->SetDirectory(dir);
//     reader->SetImageIO(dicomIO);
//     reader->SetFileNames(nameGen->GetFileNames(nameGen->GetSeriesUIDs()[0]));

//     reader->Update();
//     return reader->GetOutput();
// }

// MaskType::Pointer LoadMask(const std::string& filename)
// {
//     using ReaderType = itk::ImageFileReader<MaskType>;
//     auto reader = ReaderType::New();
//     reader->SetFileName(filename);
//     reader->Update();
//     return reader->GetOutput();
// }

// RGBImageType::Pointer OverlayMask(ImageType::Pointer image, MaskType::Pointer mask)
// {
//     using RescaleFilter = itk::RescaleIntensityImageFilter<ImageType, ImageType>;
//     using OverlayFilter = itk::LabelOverlayImageFilter<ImageType, MaskType, RGBImageType>;

//     auto rescaler = RescaleFilter::New();
//     rescaler->SetInput(image);
//     rescaler->SetOutputMinimum(0);
//     rescaler->SetOutputMaximum(255);

//     auto overlay = OverlayFilter::New();
//     overlay->SetInput(rescaler->GetOutput());
//     overlay->SetLabelImage(mask);
//     overlay->SetOpacity(0.5);
//     overlay->Update();

//     return overlay->GetOutput();
// }

// vtkSmartPointer<vtkImageViewer2> DisplaySlice(RGBImageType::Pointer itkImage, int orientation, int sliceIndex)
// {
//     using ConnectorType = itk::ImageToVTKImageFilter<RGBImageType>;
//     auto connector = ConnectorType::New();
//     connector->SetInput(itkImage);
//     connector->Update();

//     auto viewer = vtkSmartPointer<vtkImageViewer2>::New();
//     viewer->SetInputData(connector->GetOutput());
//     viewer->SetSliceOrientation(orientation);
//     viewer->SetSlice(sliceIndex);

//     auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
//     viewer->SetupInteractor(interactor);
//     viewer->Render();
//     interactor->Start();

//     return viewer;
// }

// int main()
// {
//     const std::string dicomDir = "D:\\work\\seg_algo\\liver_lung_segmentation\\dataset\\1.2.840.113619.2.55.3.1359671652.564.1739232472.303";
//     const std::string maskFile = "D:\\work\\seg_algo\\liver_lung_segmentation\\dataset\\1.2.840.113619.2.55.3.1359671652.564.1739232472.303_liver.nii";

//     auto image = LoadDICOMSeries(dicomDir);
//     auto mask = LoadMask(maskFile);

//     auto overlayImage = OverlayMask(image, mask);

//     std::cout << "Press 'A' for Axial, 'C' for Coronal, 'S' for Sagittal views.\n";
//     std::cout << "Default: Axial view\n";

//     // 默认显示 Axial
//     DisplaySlice(overlayImage, vtkImageViewer2::SLICE_ORIENTATION_XY, 50);
//     return EXIT_SUCCESS;
// }





