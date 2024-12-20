import logging

def setup_logging():
    logging.basicConfig(
        level=logging.DEBUG, # level
        format='%(asctime)s - %(filename)s:%(lineno)d - %(levelname)s - %(message)s', # fmt
        datefmt='%Y-%m-%d %H:%M:%S' # date
    )
