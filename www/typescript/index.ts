$(() => {
	let lastTs: number | null = null;
	let allMessages: { user: string, text: string, ts: number }[] = [];
	let messageQueue: { text: string, input: JQuery<HTMLElement> }[] = [];
	let sending: boolean = false;

	function receiveMessage() {
		let url = '/messages';
		if (lastTs !== null) url += '?since=' + encodeURIComponent(lastTs.toString());

		$.get(url, function (response) {
			let messages: { user: string, text: string, ts: number }[] = [];

			if (typeof response === 'string') {
				try {
					messages = JSON.parse(response);
				} catch (e) {
					console.error('Failed to parse response:', e, response);
					messages = [];
				}
			}

			else if (Array.isArray(response)) messages = response;

			if (messages.length > 0) {
				// Update lastTs based on the last message received
				lastTs = messages[messages.length - 1].ts;

				// Only add new messages
				let newCount: number = 0;
				messages.forEach(msg => {
					if (!allMessages.some(m => m.ts === msg.ts && m.user === msg.user && m.text === msg.text)) {
						allMessages.push(msg);
						newCount++;
					}
				});

				if (newCount > 0) {
					const $chatMessages = $('#chat-messages');
					$chatMessages.empty();
					allMessages.forEach(msg => {
						// Format the timestamp (in UNIX epoch milliseconds) to a readable format
						let date: Date = new Date(msg.ts);
						let timeString: string = formatDate(date);
						let messageText: string = `<span class="chat-timestamp">[${timeString}]</span> <strong>${msg.user}:</strong> ${msg.text}`;

						const $msgDiv: JQuery<HTMLElement> = $('<div>').addClass('chat-message').html(messageText);
						$chatMessages.append($msgDiv);
					});

					if ($chatMessages[0]) $chatMessages.scrollTop($chatMessages[0].scrollHeight);
				}
			}
		}, 'json');
	}

	setInterval(receiveMessage, 2000);
	receiveMessage();

	function getCookie(name: string): string {
		let match: RegExpMatchArray = document.cookie.match(new RegExp('(^| )' + name + '=([^;]+)'));
		return match ? match[2] : '';
	}

	function sendNextMessage() {
		if (sending || messageQueue.length === 0) return;
		sending = true;
		const { text, input } = messageQueue.shift();

		const safeText: string = text.replace(/\\/g, '\\\\').replace(/"/g, '\\"').replace(/\n/g, '\\n').replace(/\r/g, '\\r');

		$.get(
			'https://api.ipify.org?format=json',
			function (data) {
				const device_id: string = getCookie('device_id') || '';

				$.post({
					url: '/messages',
					contentType: 'application/json',
					data: JSON.stringify({ text: safeText, device_id, ip: data.ip }),
					success: function () {
						input.val('');
						lastTs = null;
						allMessages = [];
						setTimeout(receiveMessage, 100);
					},
					complete: function () {
						sending = false;
						sendNextMessage();
					}
				});
			}
		);
	}

	$('.chat-form').on('submit', function (e) {
		e.preventDefault();
		const input: JQuery<HTMLElement> = $(this).find('.chat-input');
		const text: string = (input.val() as string).trim();
		if (text) {
			messageQueue.push({ text, input });
			sendNextMessage();
		}
	});

	$('#change-username').on('click', function () {
		window.location.href = '/username';
	});

	function formatDate(date: Date): string {
		const now: Date = new Date();
		const day: number = date.getDate();
		const month: number = date.getMonth();
		const year: number = date.getFullYear();
		const hours: number = date.getHours();
		const minutes: number = date.getMinutes();

		let dateString: string = '';

		if (year !== now.getFullYear()) dateString = `${day}/${month + 1}/${year} `;

		else if (day !== now.getDate() || month !== now.getMonth()) dateString = `${day}/${month + 1}`;

		return dateString ? `${dateString} ${hours}:${minutes}` : `${hours}:${minutes}`;
	}
});