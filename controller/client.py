from commands import StopCommand
from commands.launch import LaunchCommand
from commands.ping import PingCommand
from config import Settings
from pipe import PipeClient


class Client:
    settings: Settings
    pipe: PipeClient

    def __init__(self, settings: Settings):
        self.settings = settings

        self.pipe = PipeClient(settings.ctl_pipe_name)
        self.pipe.write(PingCommand().model_dump_json())
        print(self.pipe.read())
        self.pipe.close()

        self.pipe = PipeClient(settings.ctl_pipe_name)
        self.pipe.write(LaunchCommand().model_dump_json())
        print(self.pipe.read())

        self.pipe = PipeClient(settings.ctl_pipe_name)
        self.pipe.write(StopCommand().model_dump_json())
        print(self.pipe.read())


def main():
    settings = Settings()
    Client(settings)


if __name__ == "__main__":
    main()
