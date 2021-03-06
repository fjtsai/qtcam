/*
 * uvccamera.cpp -- Filter e-con camera and common features of e-con camera
 * Copyright © 2015  e-con Systems India Pvt. Limited
 *
 * This file is part of Qtcam.
 *
 * Qtcam is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Qtcam is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Qtcam. If not, see <http://www.gnu.org/licenses/>.
 */

#include "uvccamera.h"
#include "QStringList"

QMap<QString, QString> uvccamera::cameraMap;
QMap<QString, QString> uvccamera::serialNumberMap;
QString uvccamera::hidNode;

int uvccamera::hid_fd;

uvccamera::uvccamera()
{

}

unsigned int uvccamera::getTickCount()
{
    struct timeval tv;
    if(gettimeofday(&tv, NULL) != 0)
        return 0;

    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

int uvccamera::findEconDevice(QStringList *econCamera,QString parameter)
{

    //QStringList tempList = *econCamera;

    emit logHandle(QtDebugMsg,"Check Devices of"+ parameter);
    cameraMap.clear();
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *dev_list_entry;
    struct udev_device *dev,*pdev;

    /* Create the udev object */
    udev = udev_new();
    if (!udev) {
        printf("Can't create udev\n");
        exit(1);
    }

    /* Create a list of the devices in the 'video4linux/hidraw' subsystem. */
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, parameter.toLatin1().data());
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    /* For each item enumerated, print out its information.
       udev_list_entry_foreach is a macro which expands to
       a loop. The loop will be executed for each member in
       devices, setting dev_list_entry to a list entry
       which contains the device's path in /sys. */
    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path;

        /* Get the filename of the /sys entry for the device
           and create a udev_device object (dev) representing it */
        path = udev_list_entry_get_name(dev_list_entry);
        dev = udev_device_new_from_syspath(udev, path);

        /* usb_device_get_devnode() returns the path to the device node
           itself in /dev. */
        // printf("Device Node Path: %s\n", udev_device_get_devnode(dev));

        /* The device pointed to by dev contains information about
           the hidraw device. In order to get information about the
           USB device, get the parent device with the
           subsystem/devtype pair of "usb"/"usb_device". This will
           be several levels up the tree, but the function will find
           it.*/
        pdev = udev_device_get_parent_with_subsystem_devtype(
                    dev,
                    "usb",
                    "usb_device");
        if (!pdev) {
            printf("Unable to find parent usb device.");
            exit(1);
        }

        /* From here, we can call get_sysattr_value() for each file
           in the device's /sys entry. The strings passed into these
           functions (idProduct, idVendor, serial, etc.) correspond
           directly to the files in the directory which represents
           the USB device. Note that USB strings are Unicode, UCS2
           encoded, but the strings returned from
           udev_device_get_sysattr_value() are UTF-8 encoded. */

        if(!strncmp(udev_device_get_sysattr_value(pdev,"idVendor"), "2560", 4)) {
            QString hid_device = udev_device_get_devnode(dev);
            QString productName = udev_device_get_sysattr_value(pdev,"product");
            QString serialNumber = udev_device_get_sysattr_value(pdev,"serial");
            if(parameter!="video4linux") {
                emit logHandle(QtDebugMsg, "HID Device found: "+productName + ": Available in: "+hid_device);
                uvccamera::cameraMap.insertMulti(productName,hid_device);
                if(serialNumber.isEmpty())
                    serialNumberMap.insertMulti(hid_device,tr("Not assigned"));
                else
                    serialNumberMap.insertMulti(hid_device,serialNumber);
            }

        } else {
            if (!strncmp(udev_device_get_sysattr_value(pdev,"idVendor"), "058f",4)) {
            }
            else {
                QString productName = udev_device_get_sysattr_value(pdev,"product");
                econCamera->removeOne(productName);
            }
        }
        udev_device_unref(dev);

    }
    /* Free the enumerator object */
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    //*econCamera = tempList;
    return 1;
}


bool uvccamera::readFirmwareVersion(quint8 *pMajorVersion, quint8 *pMinorVersion1, quint16 *pMinorVersion2, quint16 *pMinorVersion3) {


    if(uvccamera::hid_fd < 0)
    {
        return false;
    }

    bool timeout = true;
    int ret = 0;
    unsigned int start, end = 0;
    unsigned short int sdk_ver=0, svn_ver=0;

    *pMajorVersion = 0;
    *pMinorVersion1 = 0;
    *pMinorVersion2 = 0;
    *pMinorVersion3 = 0;
    //Initialize the buffer
    memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

    //Set the Report Number
    g_out_packet_buf[1] = READFIRMWAREVERSION; 	/* Report Number */

    /* Send a Report to the Device */
    ret = write(hid_fd, g_out_packet_buf, BUFFER_LENGTH);
    if (ret < 0) {
        perror("write");
        _text = tr("Device not available");
        return false;
    } else {
        printf("%s(): wrote %d bytes\n", __func__,ret);
    }
    /* Read the Firmware Version from the device */
    start = getTickCount();
    while(timeout)
    {
        /* Get a report from the device */
        ret = read(hid_fd, g_in_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            //perror("read");
        } else {
            printf("%s(): read %d bytes:\n", __func__,ret);
            if(g_in_packet_buf[0] == READFIRMWAREVERSION) {
                sdk_ver = (g_in_packet_buf[3]<<8)+g_in_packet_buf[4];
                svn_ver = (g_in_packet_buf[5]<<8)+g_in_packet_buf[6];

                *pMajorVersion = g_in_packet_buf[1];
                *pMinorVersion1 = g_in_packet_buf[2];
                *pMinorVersion2 = sdk_ver;
                *pMinorVersion3 = svn_ver;

                timeout = false;
            }
        }
        end = getTickCount();
        if(end - start > TIMEOUT)
        {
            printf("%s(): Timeout occurred\n", __func__);
            timeout = false;
            return false;
        }
    }
    return true;
}

bool uvccamera::initExtensionUnit(QString cameraName) {

    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg,"cameraName not passed as parameter\n");
        return false;
    }
    if(hid_fd >= 0)
    {
        close(hid_fd);
    }

    if(hidNode == "")
    {
        return false;
    }

    uint i;
    int ret, desc_size = 0;
    char buf[256];
    struct hidraw_devinfo info;
    struct hidraw_report_descriptor rpt_desc;


    /* Open the Device with non-blocking reads. In real life,
           don't use a hard coded path; use libudev instead. */
    QMap<QString, QString>::const_iterator ii = cameraMap.find(cameraName);
    openNode = "";
    while (ii != cameraMap.end() && ii.key() == cameraName) {
        hid_fd = open(ii.value().toLatin1().data(), O_RDWR|O_NONBLOCK);
        memset(buf, 0x0, sizeof(buf));
        /* Get Physical Location */
        ret = ioctl(hid_fd, HIDIOCGRAWPHYS(256), buf);
        if (ret < 0) {
            return false;
        }
        QString tempBuf = buf;
        if(tempBuf.contains(hidNode)) {
            openNode = ii.value();
            close(hid_fd);
            break;
        }
        close(hid_fd);
        ++ii;
    }

    hid_fd = open(openNode.toLatin1().data(), O_RDWR|O_NONBLOCK);

    //Directly open from map value
    //hid_fd = open(cameraMap.value(getCameraName()).toLatin1().data(), O_RDWR|O_NONBLOCK);

    if (hid_fd < 0) {
        perror("Unable to open device");
        return false;
    }

    memset(&rpt_desc, 0x0, sizeof(rpt_desc));
    memset(&info, 0x0, sizeof(info));
    memset(buf, 0x0, sizeof(buf));

    /* Get Report Descriptor Size */
    ret = ioctl(hid_fd, HIDIOCGRDESCSIZE, &desc_size);
    if (ret < 0) {
        perror("HIDIOCGRDESCSIZE");
        return false;
    }
    else
        printf("Report Descriptor Size: %d\n", desc_size);

    /* Get Report Descriptor */
    rpt_desc.size = desc_size;
    ret = ioctl(hid_fd, HIDIOCGRDESC, &rpt_desc);
    if (ret < 0) {
        perror("HIDIOCGRDESC");
        return false;
    } else {
        printf("Report Descriptors:\n");
        for (i = 0; i < rpt_desc.size; i++)
            printf("%hhx ", rpt_desc.value[i]);
        puts("\n");
    }

    /* Get Raw Name */
    ret = ioctl(hid_fd, HIDIOCGRAWNAME(256), buf);
    if (ret < 0) {
        perror("HIDIOCGRAWNAME");
        return false;
    }
    //    else
    //        printf("Raw Name: %s\n", buf);

    /* Get Physical Location */
    ret = ioctl(hid_fd, HIDIOCGRAWPHYS(256), buf);
    if (ret < 0) {
        perror("HIDIOCGRAWPHYS");
        return false;
    }
    //    else
    //        printf("Raw Phys: %s\n", buf);

    /* Get Raw Info */
    ret = ioctl(hid_fd, HIDIOCGRAWINFO, &info);
    if (ret < 0) {
        perror("HIDIOCGRAWINFO");
        return false;
    }

    ret = sendOSCode();
    if (ret == false) {
        printf("OS Identification failed\n");
    }
    return true;
}

void uvccamera::getDeviceNodeName(QString hidDeviceNode) {
    if(hidDeviceNode.isEmpty())
    {
        emit logHandle(QtCriticalMsg,"hid Device usbAddress Not found as parameter\n");
        return;
    }
    hidNode = hidDeviceNode;
}

void uvccamera::exitExtensionUnit() {
    close(hid_fd);
}

bool uvccamera::sendOSCode() {

    if(hid_fd < 0)
    {
        return false;
    }
    int ret = 0;
    bool timeout = true;
    unsigned int start, end = 0;

    //Initialize the buffer
    memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

    //Set the Report Number for OS identification
    g_out_packet_buf[1] = OS_CODE; 	/* Report Number OS identification */
    g_out_packet_buf[2] = LINUX_OS;	/* Report Number for Linux OS */

    /* Send a Report to the Device */
    ret = write(hid_fd, g_out_packet_buf, BUFFER_LENGTH);
    if (ret < 0) {
        perror("write");
        printf("\nOS Identification Failed\n");
        return false;
    } else {
        printf("%s(): wrote %d bytes\n", __func__,ret);
        printf("\nSent Linux OS identificaton code\n");
    }
    start = getTickCount();
    while(timeout)
    {
        /* Get a report from the device */
        ret = read(hid_fd, g_in_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            //perror("read");
        } else {
            printf("%s(): read %d bytes:\n", __func__,ret);
            if(g_in_packet_buf[0] == OS_CODE &&
                    g_in_packet_buf[1] == LINUX_OS ) {
                if(g_in_packet_buf[2] == SET_SUCCESS)
                    printf("\nSet Success\n");
                else if (g_in_packet_buf[2] == SET_FAIL){
                    printf("\nSet Failed\n");
                    return false;
                }
                else {
                    printf("\nUnknown return value\n");
                    return false;
                }
                timeout = false;
            }
        }
        end = getTickCount();
        if(end - start > TIMEOUT)
        {
            printf("%s(): Timeout occurred\n", __func__);
            printf("\nOS Identification Failed\n");
            timeout = false;
            return false;
        }
    }
    return true;
}

void uvccamera::getFirmWareVersion() {
    emit logHandle(QtDebugMsg,"Firmware version:");
    _title = tr("Firmware Version");
    quint8 MajorVersion = 0, MinorVersion1= 0;
    quint16 MinorVersion2= 0, MinorVersion3= 0;
    readFirmwareVersion(&MajorVersion, &MinorVersion1, &MinorVersion2, &MinorVersion3);
    _text.clear();
    _text.append("Version: ");
    _text.append(QString::number(MajorVersion).append(".").append(QString::number(MinorVersion1)).append(".").append(QString::number(MinorVersion2)).append(".").append(QString::number(MinorVersion3)));
    emit titleTextChanged(_title,_text);
}

void uvccamera::getSerialNumber(){
    emit serialNumber("Serial Number: "+serialNumberMap.value(openNode));
}

bool See3CAM_Control::getFlashState(quint8 *flashState, QString cameraName) {

    *flashState = 0;
    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to check flash state of camera\n");
        return false;
    }
    if(uvccamera::hid_fd < 0)
    {
        return false;
    }

    bool timeout = true;
    int ret =0;
    unsigned int start, end = 0;
    //Initialize the buffer
    memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

    //Set the Report Number
    if(cameraName == "e-con's 8MP Camera")
        g_out_packet_buf[1] = CAMERA_CONTROL_80; /* Report Number */
    else if(cameraName == "See3CAMCU50")
        g_out_packet_buf[1] = CAMERA_CONTROL_50; /* Report Number */
    //g_out_packet_buf[1] = CAMERA_CONTROL_80; /* Report Number */
    g_out_packet_buf[2] = GET_FLASH_LEVEL; /* Report Number */

    qDebug()<<"Hid file descriptor::"<<hid_fd;
    ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);

    if (ret < 0) {
        perror("write");
        return false;
    } else {
        printf("%s(): write() wrote %d bytes\n", __func__, ret);
    }
    /* Read the Status code from the device */
    start = getTickCount();
    while(timeout)
    {
        /* Get a report from the device */
        ret = read(uvccamera::hid_fd, g_in_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            //perror("read");
        } else {
            printf("%s(): read %d bytes:\n", __func__,ret);
            if((g_in_packet_buf[0] == g_out_packet_buf[1])&&
                    (g_in_packet_buf[1]==GET_FLASH_LEVEL)) {
                *flashState = (g_in_packet_buf[2]);
                timeout=false;
            }
        }
        end = getTickCount();
        if(end - start > TIMEOUT)
        {
            printf("%s(): Timeout occurred\n", __func__);
            timeout = false;
            return false;
        }
    }
    return true;
}

bool See3CAM_Control::setFlashState(flashTorchState flashState, QString cameraName)
{

    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to set flash state of camera\n");
        return false;
    }
    if(uvccamera::hid_fd < 0)
    {
        return false;
    }

    bool timeout = true;
    int ret =0;
    unsigned int start, end = 0;


    if(flashState == flashOff || flashState == flashOn)
    {
        //Initialize the buffer
        memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));
        //Set the Report Number
        if(cameraName == "e-con's 8MP Camera")
            g_out_packet_buf[1] = CAMERA_CONTROL_80; /* Report Number */
        else if(cameraName == "See3CAMCU50")
            g_out_packet_buf[1] = CAMERA_CONTROL_50; /* Report Number */
        g_out_packet_buf[2] = SET_FLASH_LEVEL; 	/* Report Number */
        g_out_packet_buf[3] = flashState;		/* Flash mode */

        ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);

        if (ret < 0) {
            perror("write");
            return false;
        } else {
            printf("%s(): write() wrote %d bytes\n", __func__, ret);
        }
        /* Read the Status code from the device */
        start = getTickCount();
        while(timeout)
        {
            /* Get a report from the device */
            ret = read(uvccamera::hid_fd, g_in_packet_buf, BUFFER_LENGTH);
            if (ret < 0) {
                //perror("read");
            } else {
                printf("%s(): read %d bytes:\n", __func__,ret);
                if((g_in_packet_buf[0] == g_out_packet_buf[1])&&
                        (g_in_packet_buf[1]==SET_FLASH_LEVEL) &&
                        (g_in_packet_buf[2]==flashState )) {
                    if(g_in_packet_buf[3] == SET_FAIL) {
                        return false;
                    } else if(g_in_packet_buf[3]==SET_SUCCESS) {
                        timeout = false;
                    }
                }
            }
            end = getTickCount();
            if(end - start > TIMEOUT)
            {
                printf("%s(): Timeout occurred\n", __func__);
                timeout = false;
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

bool See3CAM_Control::getTorchState(quint8 *torchState, QString cameraName) {

    *torchState = 0;
    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to get torch state of camera\n");
        return false;
    }
    if(uvccamera::hid_fd < 0)
    {
        return false;
    }

    bool timeout = true;
    int ret =0;
    unsigned int start, end = 0;
    //Initialize the buffer
    memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

    //Set the Report Number
    if(cameraName == "e-con's 8MP Camera")
        g_out_packet_buf[1] = CAMERA_CONTROL_80; /* Report Number */
    else if(cameraName == "See3CAMCU50")
        g_out_packet_buf[1] = CAMERA_CONTROL_50; /* Report Number */
    g_out_packet_buf[2] = GET_TORCH_LEVEL; /* Report Number */

    ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);
    if (ret < 0) {
        perror("write");
        return false;
    } else {
        printf("%s(): write() wrote %d bytes\n", __func__, ret);
    }
    /* Read the Status code from the device */
    start = getTickCount();
    while(timeout)
    {
        /* Get a report from the device */
        ret = read(hid_fd, g_in_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            //perror("read");
        } else {
            printf("%s(): read %d bytes:\n", __func__,ret);
            if((g_in_packet_buf[0] == g_out_packet_buf[1])&&
                    (g_in_packet_buf[1] == GET_TORCH_LEVEL)) {
                *torchState = (g_in_packet_buf[2]);
                timeout=false;
            }
        }
        end = getTickCount();
        if(end - start > TIMEOUT)
        {
            printf("%s(): Timeout occurred\n", __func__);
            timeout = false;
            return false;
        }
    }
    return true;
}

bool See3CAM_Control::setTorchState(flashTorchState torchState, QString cameraName)
{
    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to set torch state of camera\n");
        return false;
    }
    if(uvccamera::hid_fd < 0)
    {
        return false;
    }

    bool timeout = true;
    int ret =0;
    unsigned int start, end = 0;

    if(torchState == torchOff || torchState == torchOn)
    {
        //Initialize the buffer
        memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

        //Set the Report Number
        if(cameraName == "e-con's 8MP Camera")
            g_out_packet_buf[1] = CAMERA_CONTROL_80; /* Report Number */
        else if(cameraName == "See3CAMCU50")
            g_out_packet_buf[1] = CAMERA_CONTROL_50; /* Report Number */
        g_out_packet_buf[2] = SET_TORCH_LEVEL; 	/* Report Number */
        g_out_packet_buf[3] = torchState;		/* Flash mode */

        ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);

        if (ret < 0) {
            perror("write");
            return false;
        } else {
            printf("%s(): write() wrote %d bytes\n", __func__, ret);
        }
        /* Read the Status code from the device */
        start = getTickCount();
        while(timeout)
        {
            /* Get a report from the device */
            ret = read(uvccamera::hid_fd, g_in_packet_buf, BUFFER_LENGTH);
            if (ret < 0) {
                //perror("read");
            } else {
                printf("%s(): read %d bytes:\n", __func__,ret);
                if((g_in_packet_buf[0] == g_out_packet_buf[1])&&
                        (g_in_packet_buf[1]==SET_TORCH_LEVEL) &&
                        (g_in_packet_buf[2]==torchState )) {
                    if(g_in_packet_buf[3] == SET_FAIL) {
                        return false;
                    } else if(g_in_packet_buf[3]==SET_SUCCESS) {
                        timeout = false;
                    }
                }
            }
            end = getTickCount();
            if(end - start > TIMEOUT)
            {
                printf("%s(): Timeout occurred\n", __func__);
                timeout = false;
                return false;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}

void See3CAM_Control::setFlashControlState(const int flashState,QString cameraName)
{

    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to set flash control state of camera\n");
        return void();
    }
    if(flashState == 1)
        flashCheckBoxState = flashOn;
    else
        flashCheckBoxState = flashOff;
    setFlashState(flashCheckBoxState,cameraName);
}

void See3CAM_Control::setTorchControlState(const int torchState,QString cameraName) {
    if(cameraName.isEmpty())
    {
        emit logHandle(QtCriticalMsg," cameraName Not passed to set torch control state of camera\n");
        return void();
    }
    if(torchState == 1)
        torchCheckBoxState = torchOn;
    else
        torchCheckBoxState = torchOff;
    setTorchState(torchCheckBoxState,cameraName);
}

void See3CAM_GPIOControl::getGpioLevel(camGpioPin gpioPinNumber)
{

    if(uvccamera::hid_fd < 0)
    {
        return void();
    }
    bool timeout = true;
    int ret = 0;
    unsigned int start, end = 0;

    if(gpioPinNumber == OUT1 || gpioPinNumber == OUT2 || gpioPinNumber == IN1 || gpioPinNumber == IN2 || gpioPinNumber == IN3 )
    {
        //Initialize the buffer
        memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

        //Set the Report Number
        g_out_packet_buf[1] = GPIO_OPERATION; 	/* Report Number */
        g_out_packet_buf[2] = GPIO_GET_LEVEL; 	/* Report Number */
        g_out_packet_buf[3] = gpioPinNumber; 		/* GPIO Pin Number */
        /* Send a Report to the Device */
        ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            perror("write");
            return void();
        } else {
            printf("%s(): wrote %d bytes\n", __func__,ret);
        }
        /* Read the GPIO level and status of read from the device */
        start = uvc.getTickCount();
        while(timeout)
        {
            /* Get a report from the device */
            ret = read(uvccamera::hid_fd, g_in_packet_buf, BUFFER_LENGTH);

            if (ret < 0) {
                //perror("read");
            } else {
                printf("%s(): read %d bytes:\n", __func__,ret);
                if(g_in_packet_buf[0] == GPIO_OPERATION &&
                        g_in_packet_buf[1] == GPIO_GET_LEVEL &&
                        g_in_packet_buf[2] == gpioPinNumber) {
                    emit gpioLevel(g_in_packet_buf[3]);
                    if(g_in_packet_buf[4] == GPIO_LEVEL_FAIL) {
                        return void();
                    } else if(g_in_packet_buf[4]==GPIO_LEVEL_SUCCESS) {
                        timeout = false;
                    }
                }
            }
            end = uvc.getTickCount();
            if(end - start > TIMEOUT)
            {
                printf("%s(): Timeout occurred\n", __func__);
                timeout = false;
                return void();
            }
        }
    }
    else
    {
        return void();
    }
}

void See3CAM_GPIOControl::setGpioLevel(camGpioPin gpioPin,camGpioValue gpioValue)
{

    if(uvccamera::hid_fd < 0)
    {
        return void();
    }

    bool timeout = true;
    int ret = 0;
    unsigned int start, end = 0;

    if((gpioPin == OUT1 || gpioPin == OUT2))
    {
        if((gpioValue == High || gpioValue == Low))
        {
            //Initialize the buffer
            memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

            //Set the Report Number
            g_out_packet_buf[1] = GPIO_OPERATION; 	/* Report Number */
            g_out_packet_buf[2] = GPIO_SET_LEVEL; 	/* Report Number */
            g_out_packet_buf[3] = gpioPin; 		/* GPIO Pin Number */
            g_out_packet_buf[4] = gpioValue; 	/* GPIO Value */

            /* Send a Report to the Device */
            ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);
            if (ret < 0) {
                perror("write");
                return void();
            } else {
                printf("%s(): wrote %d bytes\n", __func__,ret);
            }
            /* Read the GPIO level and status of read from the device */
            start = uvc.getTickCount();
            while(timeout)
            {
                /* Get a report from the device */
                ret = read(uvccamera::hid_fd, g_in_packet_buf, BUFFER_LENGTH);
                if (ret < 0) {
                    //perror("read");
                } else {
                    printf("%s(): read %d bytes:\n", __func__,ret);
                    if(g_in_packet_buf[0] == GPIO_OPERATION &&
                            g_in_packet_buf[1] == GPIO_SET_LEVEL &&
                            g_in_packet_buf[2] == gpioPin &&
                            g_in_packet_buf[3] == gpioValue) {
                        if(g_in_packet_buf[4] == GPIO_LEVEL_FAIL) {
                            emit deviceStatus(tr("Failure"), tr("Unable to change the GPIO level"));
                            return void();
                        } else if(g_in_packet_buf[4]==GPIO_LEVEL_SUCCESS) {
                            timeout = false;
                        }
                    }
                }
                end = uvc.getTickCount();
                if(end - start > TIMEOUT)
                {
                    printf("%s(): Timeout occurred\n", __func__);
                    timeout = false;
                    return void();
                }
            }
        }
            else
            {
                return void();
            }
        }
        else
        {
            return void();
        }
    }

    bool See3CAM_ModeControls::enableMasterMode()
    {
        int ret =0;

        if(uvccamera::hid_fd < 0)
        {
            return false;
        }
        //Initialize the buffer
        memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

        //Set the Report Number
        g_out_packet_buf[1] = ENABLEMASTERMODE; /* Report Number */

        ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            perror("write");
            return false;
        } else {
            printf("%s(): write() wrote %d bytes\n", __func__, ret);
        }
        return true;
    }

    bool See3CAM_ModeControls::enableTriggerMode()
    {
        int ret =0;

        if(uvccamera::hid_fd < 0)
        {
            return false;
        }
        //Initialize the buffer
        memset(g_out_packet_buf, 0x00, sizeof(g_out_packet_buf));

        //Set the Report Number
        g_out_packet_buf[1] = ENABLETRIGGERMODE; /* Report Number */

        ret = write(uvccamera::hid_fd, g_out_packet_buf, BUFFER_LENGTH);
        if (ret < 0) {
            perror("write");
            return false;
        } else {
            printf("%s(): write() wrote %d bytes\n", __func__, ret);
        }
        return true;
    }






