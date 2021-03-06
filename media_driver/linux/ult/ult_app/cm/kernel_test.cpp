/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "cm_test.h"

using CMRT_UMD::MockDevice;
class KernelTest: public CmTest
{
public:
    KernelTest(): m_program(nullptr), m_kernel(nullptr) {}

    ~KernelTest() {}

    bool CreateKernel(MockDevice *device)
    {
        int32_t result1 = (*device)->CreateKernel(m_program, "kernel", m_kernel,
                                                 nullptr);
        EXPECT_EQ(CM_SUCCESS, result1);
        int32_t result2 = (*device)->DestroyKernel(m_kernel);
        EXPECT_EQ(CM_SUCCESS, result2);
        return CM_SUCCESS == result1 && CM_SUCCESS == result2;
    }//=======================================================

    int32_t LoadDestroy(void *isa_code, uint32_t size)
    {
        MockDevice mock_device(&m_driverLoader);
        int32_t result = mock_device->LoadProgram(isa_code, size, m_program,
                                                  "nojitter");
        if (result != CM_SUCCESS)
        {
            return result;
        }
        CreateKernel(&mock_device);
        return mock_device->DestroyProgram(m_program);
    }//===============================================
  
private:
    CMRT_UMD::CmProgram *m_program;
    CMRT_UMD::CmKernel *m_kernel;
};//=============================

TEST_F(KernelTest, LoadDestroy)
{
    RunEach(CM_INVALID_COMMON_ISA,
            [this]() { return LoadDestroy(nullptr, 0); });

    uint8_t isa_code[]
        = {0x43, 0x49, 0x53, 0x41, 0x03, 0x06, 0x01, 0x00, 0x06, 0x6b, 0x65,
           0x72, 0x6e, 0x65, 0x6c, 0x2d, 0x00, 0x00, 0x00, 0xb4, 0x01, 0x00,
           0x00, 0x8d, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05,
           0xe1, 0x01, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x1f, 0x00, 0x00, 0x00, 0x6b, 0x65, 0x72, 0x6e, 0x65, 0x6c,
           0x00, 0x6e, 0x75, 0x6c, 0x6c, 0x00, 0x74, 0x68, 0x72, 0x65, 0x61,
           0x64, 0x5f, 0x78, 0x00, 0x74, 0x68, 0x72, 0x65, 0x61, 0x64, 0x5f,
           0x79, 0x00, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x5f, 0x69, 0x64, 0x5f,
           0x78, 0x00, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x5f, 0x69, 0x64, 0x5f,
           0x79, 0x00, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x5f, 0x69, 0x64, 0x5f,
           0x7a, 0x00, 0x74, 0x73, 0x63, 0x00, 0x72, 0x30, 0x00, 0x61, 0x72,
           0x67, 0x00, 0x72, 0x65, 0x74, 0x76, 0x61, 0x6c, 0x00, 0x73, 0x70,
           0x00, 0x66, 0x70, 0x00, 0x68, 0x77, 0x5f, 0x69, 0x64, 0x00, 0x73,
           0x72, 0x30, 0x00, 0x63, 0x72, 0x30, 0x00, 0x63, 0x65, 0x30, 0x00,
           0x64, 0x62, 0x67, 0x30, 0x00, 0x63, 0x6f, 0x6c, 0x6f, 0x72, 0x00,
           0x54, 0x30, 0x00, 0x54, 0x31, 0x00, 0x54, 0x32, 0x00, 0x54, 0x33,
           0x00, 0x54, 0x32, 0x35, 0x32, 0x00, 0x54, 0x32, 0x35, 0x35, 0x00,
           0x53, 0x33, 0x31, 0x00, 0x6b, 0x65, 0x72, 0x6e, 0x65, 0x6c, 0x5f,
           0x42, 0x42, 0x5f, 0x30, 0x5f, 0x31, 0x00, 0x41, 0x73, 0x6d, 0x4e,
           0x61, 0x6d, 0x65, 0x00, 0x4e, 0x6f, 0x42, 0x61, 0x72, 0x72, 0x69,
           0x65, 0x72, 0x00, 0x54, 0x61, 0x72, 0x67, 0x65, 0x74, 0x00, 0x44,
           0x3a, 0x5c, 0x63, 0x79, 0x67, 0x77, 0x69, 0x6e, 0x36, 0x34, 0x5c,
           0x68, 0x6f, 0x6d, 0x65, 0x5c, 0x73, 0x68, 0x65, 0x6e, 0x67, 0x63,
           0x6f, 0x6e, 0x5c, 0x64, 0x65, 0x76, 0x5f, 0x6d, 0x65, 0x64, 0x69,
           0x61, 0x5c, 0x6d, 0x64, 0x66, 0x5f, 0x63, 0x6f, 0x6e, 0x66, 0x6f,
           0x72, 0x6d, 0x61, 0x6e, 0x63, 0x65, 0x5f, 0x74, 0x65, 0x73, 0x74,
           0x73, 0x5c, 0x54, 0x61, 0x73, 0x6b, 0x5c, 0x43, 0x6d, 0x50, 0x72,
           0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5c, 0x30, 0x30, 0x38, 0x5f, 0x43,
           0x6d, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5f, 0x5a, 0x65,
           0x72, 0x6f, 0x43, 0x6f, 0x64, 0x65, 0x53, 0x69, 0x7a, 0x65, 0x5c,
           0x54, 0x4d, 0x50, 0x5f, 0x44, 0x49, 0x52, 0x5c, 0x30, 0x30, 0x38,
           0x5f, 0x43, 0x6d, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d, 0x5f,
           0x5a, 0x65, 0x72, 0x6f, 0x43, 0x6f, 0x64, 0x65, 0x53, 0x69, 0x7a,
           0x65, 0x5f, 0x67, 0x65, 0x6e, 0x78, 0x2e, 0x63, 0x70, 0x70, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
           0x00, 0x01, 0x00, 0x1a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
           0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0xa3, 0x01,
           0x00, 0x00, 0x03, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x25, 0x30, 0x30,
           0x38, 0x5f, 0x43, 0x6d, 0x50, 0x72, 0x6f, 0x67, 0x72, 0x61, 0x6d,
           0x5f, 0x5a, 0x65, 0x72, 0x6f, 0x43, 0x6f, 0x64, 0x65, 0x53, 0x69,
           0x7a, 0x65, 0x5f, 0x67, 0x65, 0x6e, 0x78, 0x5f, 0x30, 0x2e, 0x61,
           0x73, 0x6d, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x00, 0x00, 0x00,
           0x01, 0x00, 0x30, 0x00, 0x00, 0x51, 0x1e, 0x00, 0x00, 0x00, 0x52,
           0x05, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00, 0x01, 0x4d, 0x00,
           0x20, 0x07, 0x7f, 0x00, 0x00, 0x31, 0x00, 0x00, 0x07, 0x00, 0x02,
           0x00, 0x20, 0xe0, 0x0f, 0x00, 0x06, 0x10, 0x00, 0x00, 0x82};

    RunEach(CM_INVALID_COMMON_ISA,
            [this, isa_code]()
                { return LoadDestroy(const_cast<uint8_t*>(isa_code), 0); });

    // This CISA binary is valid on only 1 platform.
    auto LoadDestroyTrueBinary = [this, isa_code]()
        { int32_t result = LoadDestroy(const_cast<uint8_t*>(isa_code),
                                       sizeof(isa_code));
          return CM_SUCCESS == result || CM_INVALID_GENX_BINARY == result;};
    RunEach(true, LoadDestroyTrueBinary);
    return;
}//========
