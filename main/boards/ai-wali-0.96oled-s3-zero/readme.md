# 自定义开发板流程，参见docs/custom-board_zh.md
本board基于xingzhi-cube-0.96oled-wifi拓展
上述板子内容较少，无过多配置，适合二次开发
power_manager.h为xingzhi-cube-1.54tft-wifi目录下，原0.96oled引用了这个文件，直接复制过来了
针对power_manager.h中的ReadBatteryAdcData()函数有改动，将电池adc数值低于1000时设置为满电（没有电池但是空气中的adc一般在几百，有点池时1700基本就是没电状态了），避免没有电池情况下一直提示需要充电
注意BOARD_TYPE_AI...的格式，必须是大写、下划线，另外前边必须带BOARD_TYPE_ ！！！（血的教训）