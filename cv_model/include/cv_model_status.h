#ifndef CV_MODEL_STATUS_H
#define CV_MODEL_STATUS_H

extern bool cvModelReadyFlag; // cv_model准备就绪标志，cv_model在完成初始化后将该标志设置为true，core可以根据该标志判断cv_model是否准备就绪
extern bool cvModelStaticRecognitionFlag; // cv_model静态识别标志，core设置为true表示进行静态识别，cv_model识别完成后将该标志重置为false
extern bool cvModelDynamicRecognitionFlag; // cv_model动态识别标志，core设置为true表示进行动态识别，cv_model识别完成后将该标志重置为false

#endif // CV_MODEL_STATUS_H