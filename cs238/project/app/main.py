from apscheduler.schedulers.background import BackgroundScheduler
from config import USERS_CREDS
from model import *
from services.flask_app import app, db
from tasks.select_distractor_tweet import select_distractor_tweet
from tasks.select_tweets import select_tweets_popular, select_tweets_recent, select_tweets_home_timeline
import datetime
from tasks.delete_old_tweets import delete_old_replies
from tasks.record_user_events import record_user_events
from tasks.update_q_table import update_q_table

DISTRACTOR_TWEET_INTERVAL_MINUTES = 5
DELETE_INTERVAL_MINUTES = 30
MAX_REPLY_AGE = datetime.timedelta(hours=48)
UPDATE_Q_TABLE_INTERVAL_MINUTES = 3

sched = BackgroundScheduler()

if __name__ == '__main__':
    db.create_all()
    sched.add_job(select_tweets_popular, 'interval', minutes=5,
                next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=30))
    sched.add_job(select_tweets_recent)
    sched.add_job(select_tweets_home_timeline, 'interval', minutes=5,
            next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=60))
    for user_creds in USERS_CREDS:
        sched.add_job(select_distractor_tweet, 'interval', 
            minutes=DISTRACTOR_TWEET_INTERVAL_MINUTES,
            args=[user_creds])
        sched.add_job(record_user_events, args = [user_creds])
        sched.add_job(delete_old_replies, 'interval', 
            minutes=DELETE_INTERVAL_MINUTES,
            args=[user_creds, MAX_REPLY_AGE],
            next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=5))

    sched.add_job(update_q_table, 'interval', 
        minutes=UPDATE_Q_TABLE_INTERVAL_MINUTES,
        next_run_time = datetime.datetime.now() + datetime.timedelta(seconds=5))

    sched.start()
    app.run(host='127.0.0.1', port=8080, debug=True, use_reloader=False)