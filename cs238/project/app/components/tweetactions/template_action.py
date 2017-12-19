from jinja2 import Template
import os

dir = os.path.dirname(__file__)

class TemplateAction:

	def __init__(self, name, template_name):
		self.name = name
		self.template = self.get_template(template_name)

	def get_tweet_text(self, tweet):
		return self.template.render()

	def get_template(self, template_name):
		file = open(os.path.join(dir, 'templates', template_name), 'r')
		return Template(file.read())