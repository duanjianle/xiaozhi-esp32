# 音频服务架构 (Audio Service Architecture)
音频服务（Audio Service）是一个核心组件，负责管理所有与音频相关的功能，包括从麦克风采集音频、音频处理、编码/解码，以及通过扬声器播放音频。它采用模块化和高效的设计，其主要操作运行在专属的 FreeRTOS 任务中，以确保实时性能。
## 核心组件 (Key Components)
•	AudioService：中央调度器（Orchestrator）。它负责初始化并管理所有其他音频组件、FreeRTOS 任务和数据队列。
•	AudioCodec：物理音频编解码芯片的硬件抽象层（HAL）。它负责处理用于音频输入和输出的原始 I2S 通信。
•	AudioProcessor：对麦克风输入流进行实时音频处理。这通常包括声学回声消除（AEC）、噪声抑制（NS）和语音活动检测（VAD）。AfeAudioProcessor 是默认的实现方案，它利用了 ESP-ADF 的音频前处理（Audio Front-End）框架。
•	WakeWord：从音频流中检测唤醒词（例如 “你好，小智”、“Hi, ESP”）。在检测到唤醒词之前，它独立于主音频处理器运行。
•	OpusEncoderWrapper / OpusDecoderWrapper：管理将 PCM 音频编码为 Opus 格式，以及将 Opus 数据包解码回 PCM 音频。Opus 格式因其高压缩率和低延迟特性，成为语音流媒体的理想选择。
•	OpusResampler：用于在不同采样率之间转换音频流的工具（例如，将编解码芯片的原生采样率重采样为处理所需的 16kHz）。
## 线程模型 (Threading Model)
该服务运行在三个主要任务上，以并发方式处理音频流水线的不同阶段：
	1.	AudioInputTask：专门负责从 AudioCodec 中读取原始 PCM 数据。然后，它根据当前系统状态，将该数据送入 WakeWord 引擎或 AudioProcessor。
	2.	AudioOutputTask：负责音频播放。它从 audio_playback_queue_（音频播放队列）中获取解码后的 PCM 数据，并将其发送至 AudioCodec 以通过扬声器播放。
	3.	OpusCodecTask：处理编码和解码的辅助工作任务（Worker Task）。它从 audio_encode_queue_（音频编码队列）中获取原始音频，将其编码为 Opus 数据包，并存入 audio_send_queue_（音频发送队列）；同时，它从 audio_decode_queue_（音频解码队列）中获取 Opus 数据包，将其解码为 PCM 数据，并存入 audio_playback_queue_（音频播放队列）。
## 数据流向 (Data Flow)
主要包含两种数据流：音频输入（上行）和音频输出（下行）。
### 1. 音频输入（上行）流向
该流程捕获麦克风音频、对其进行处理和编码，并准备将其发送至服务器。
graph TD
    subgraph Device [设备端]
        Mic[("麦克风")] -->|I2S| Codec(AudioCodec 音频编解码器)
        
        subgraph AudioInputTask [音频输入任务]
            Codec -->|原始 PCM| Read(ReadAudioData 读取音频数据)
            Read -->|16kHz PCM| Processor(AudioProcessor 音频处理器)
        end

        subgraph OpusCodecTask [Opus 编解码任务]
            Processor -->|纯净 PCM| EncodeQueue(audio_encode_queue_ 编码队列)
            EncodeQueue --> Encoder(OpusEncoder 编码器)
            Encoder -->|Opus 数据包| SendQueue(audio_send_queue_ 发送队列)
        end

        SendQueue --> |"PopPacketFromSendQueue()"| App(应用层)
    end
    
    App -->|网络| Server((云端服务器))

•	AudioInputTask 持续从 AudioCodec 中读取原始 PCM 数据。
•	该数据被送入 AudioProcessor 进行降噪清理（AEC 消除回声、VAD 语音检测）。
•	处理后的干净 PCM 数据被推入 audio_encode_queue_。
•	OpusCodecTask 提取该 PCM 数据，将其编码为 Opus 格式，并将生成的压缩数据包推入 audio_send_queue_。
•	随后，应用层可以从发送队列中取出这些 Opus 数据包并通过网络发送出去。
### 2. 音频输出（下行）流向
该流程接收已编码的音频数据、对其进行解码，并通过扬声器进行播放。
graph TD
    Server((云端服务器)) -->|网络| App(应用层)

    subgraph Device [设备端]
        App -->|"PushPacketToDecodeQueue()"| DecodeQueue(audio_decode_queue_ 解码队列)

        subgraph OpusCodecTask [Opus 编解码任务]
            DecodeQueue -->|Opus 数据包| Decoder(OpusDecoder 解码器)
            Decoder -->|PCM| PlaybackQueue(audio_playback_queue_ 播放队列)
        end

        subgraph AudioOutputTask [音频输出任务]
            PlaybackQueue -->|PCM| Codec(AudioCodec 音频编解码器)
        end

        Codec -->|I2S| Speaker[("扬声器")]
    end

•	应用层从网络接收到 Opus 数据包，并将其推入 audio_decode_queue_。
•	OpusCodecTask 提取这些数据包，将它们解码回原始 PCM 数据，然后将数据推入 audio_playback_queue_。
•	AudioOutputTask 从该播放队列中获取 PCM 数据，并将其发送至 AudioCodec 进行硬件级放音。
## 电源管理 (Power Management)
为了节省能源，音频编解码芯片的输入（ADC）和输出（DAC）通道在闲置一段时间（由 AUDIO_POWER_TIMEOUT_MS 设定）后会自动禁用。系统通过一个定时器（audio_power_timer_）定期检查音频活动并管理电源状态。当有新的音频需要捕获或播放时，这些通道会自动重新启用。