$(() => {
	const form: JQuery<HTMLElement> = $('.username-form');

	if (!form[0]) {
		console.error('Username form not found');
		return;
	}

	$(form).on('submit', function (e) {
		e.preventDefault();

		const username: string = ($(form).find('.username-input').val() as string).trim();

		if (!username) return alert('Username cannot be empty');

		$.post({
			url: '/username',
			contentType: 'application/json',
			data: JSON.stringify({ username }),
			success: function () {
				window.location.href = '/';
			},
			error: function (xhr) {
				const response: string = xhr.responseText;
				if (response) alert(response);
				else {
					alert('Failed to set username, please try again.');
					console.error('Failed to set username, please try again.');
				}
			},
			complete: function () {
				$(form).find('.username-input').val('');
			}
		});
	});
});
