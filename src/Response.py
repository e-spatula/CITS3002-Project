from Frame import Frame


class Response:
    # Responses we're waiting on before we can respond
    remaining_responses: int
    sender: str
    origin: str
    seqno: int
    time: int
    stop: str

    def __init__(
        self,
        remaining_responses: int,
        sender: str,
        origin: str,
        seqno: int,
        time: int,
        stop: str,
    ):
        self.remaining_responses = remaining_responses
        self.sender = sender
        self.origin = origin
        self.seqno = seqno
        self.time = time
        self.stop = stop

    def __str__(self):
        return (
            f"Remaining Responses: {self.remaining_responses}\n"
            f"Sender: {self.sender}\n"
            f"Origin: {self.origin}\n"
            f"Seqno: {self.seqno}\n"
            f"Best time: {self.time}\n"
            f"Best stop: {self.stop}\n"
        )
